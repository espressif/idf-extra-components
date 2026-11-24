/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * Encrypted payloads, draft-ietf-suit-firmware-encryption.
 *
 * suit-parameter-encryption-info (19) carries SUIT_Encryption_Info, which is #6.96(COSE_Encrypt)
 * with a detached ciphertext. The payload itself arrives through the usual getter, so nothing in
 * the transport changes: only the bytes between the getter and the sink do.
 *
 *   recipient   ECDH-ES with the device's static key, then HKDF-SHA256, then AES key unwrap
 *   content     AES-CTR, whose integrity comes from the image digest, or AES-GCM, which has a tag
 *
 * The image digest covers the plaintext. That is what the device writes into the slot, so it is
 * also what suit-condition-image-match reads back and hashes later. The image size covers the
 * fetched resource, so for AES-GCM it includes the 16-byte tag, which is never written.
 *
 * The device's private key need not be in memory. The configuration gives either the raw scalar
 * or an agreement callback, so a production build can keep the key in eFuse or in the Key
 * Manager and let the hardware do the agreement.
 */
#include <string.h>
#include "psa/crypto.h"
#include "suit_priv.h"

#if CONFIG_ESP_SUIT_OTA_ENCRYPTED_PAYLOAD

static const char *TAG = SUIT_TAG;

/* ---------- a CBOR writer, only the few items the two signed contexts need ---------- */

static size_t put_head(uint8_t *out, uint8_t major, uint64_t v)
{
    if (v < 24) {
        out[0] = (uint8_t)(major << 5 | v);
        return 1;
    }
    if (v <= 0xFF) {
        out[0] = (uint8_t)(major << 5 | 24);
        out[1] = (uint8_t)v;
        return 2;
    }
    if (v <= 0xFFFF) {
        out[0] = (uint8_t)(major << 5 | 25);
        out[1] = (uint8_t)(v >> 8);
        out[2] = (uint8_t)v;
        return 3;
    }
    out[0] = (uint8_t)(major << 5 | 26);
    out[1] = (uint8_t)(v >> 24);
    out[2] = (uint8_t)(v >> 16);
    out[3] = (uint8_t)(v >> 8);
    out[4] = (uint8_t)v;
    return 5;
}

static size_t put_int(uint8_t *out, int64_t v)
{
    return v < 0 ? put_head(out, 1, (uint64_t)(-1 - v)) : put_head(out, 0, (uint64_t)v);
}

static size_t put_str(uint8_t *out, uint8_t major, const void *p, size_t len)
{
    size_t n = put_head(out, major, len);
    if (len > 0) {
        memcpy(out + n, p, len);
    }
    return n + len;
}

/* ---------- the two structures that must be byte identical on both sides ---------- */

/* COSE_KDF_Context of RFC 9053 section 5.2, with the values draft section 5.2.4 fixes. */
static esp_err_t kdf_context(int64_t kw_alg, const uint8_t *prot, size_t prot_len,
                             uint8_t *out, size_t cap, size_t *out_len)
{
    static const char OTHER[] = "SUIT Payload Encryption";
    const size_t other_len = sizeof(OTHER) - 1;
    if (cap < prot_len + other_len + SUIT_KDF_CONTEXT_OVERHEAD) {
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
    size_t n = 0;
    out[n++] = 0x84;                                   /* [ AlgorithmID, PartyU, PartyV, SuppPubInfo ] */
    n += put_int(out + n, kw_alg);
    for (int party = 0; party < 2; party++) {          /* both identities are empty, per the draft */
        out[n++] = 0x83;
        out[n++] = 0xf6;
        out[n++] = 0xf6;
        out[n++] = 0xf6;
    }
    out[n++] = 0x83;                                   /* [ keyDataLength, protected, other ] */
    n += put_head(out + n, 0, (uint64_t)suit_kw_key_bytes(kw_alg) * 8);
    n += put_str(out + n, 2, prot, prot_len);
    n += put_str(out + n, 2, OTHER, other_len);
    *out_len = n;
    return ESP_OK;
}

/* Enc_structure of RFC 9052 section 5.3. The draft fixes external_aad to zero length. */
static esp_err_t enc_structure(const uint8_t *prot, size_t prot_len,
                               uint8_t *out, size_t cap, size_t *out_len)
{
    static const char CONTEXT[] = "Encrypt";
    if (cap < prot_len + sizeof(CONTEXT) + SUIT_ENC_STRUCTURE_OVERHEAD) {
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
    size_t n = 0;
    out[n++] = 0x83;
    n += put_str(out + n, 3, CONTEXT, sizeof(CONTEXT) - 1);
    n += put_str(out + n, 2, prot, prot_len);
    n += put_str(out + n, 2, NULL, 0);
    *out_len = n;
    return ESP_OK;
}

/* ---------- COSE header reading ---------- */

size_t suit_kw_key_bytes(int64_t kw_alg)
{
    return kw_alg == SUIT_COSE_ALG_A256KW ? 32 : 16;
}

size_t suit_content_key_bytes(int64_t alg)
{
    return (alg == SUIT_COSE_ALG_A256GCM || alg == SUIT_COSE_ALG_A256CTR) ? 32 : 16;
}

/*
 * Draft section 11: the recipient verifies that the algorithms in the COSE_Encrypt and in each
 * recipient match what it is configured to expect, and rejects the message otherwise. This
 * stops a downgrade, for example an AEAD cipher swapped for one without integrity. An empty
 * list means no policy was given, and then every algorithm the build supports is accepted.
 */
bool suit_alg_allowed(const int64_t *allowed, size_t count, int64_t alg)
{
    if (allowed == NULL || count == 0) {
        return true;
    }
    for (size_t i = 0; i < count; i++) {
        if (allowed[i] == alg) {
            return true;
        }
    }
    return false;
}

/* the algorithm of a serialized protected header, or ESP_ERR_NOT_FOUND when it has none */
static esp_err_t header_alg(const uint8_t *prot, size_t prot_len, int64_t *alg)
{
    if (prot_len == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    suit_cbor_t c;
    suit_cbor_init(&c, prot, prot_len);
    size_t count = 0;
    if (suit_cbor_get_map(&c, &count) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    for (size_t i = 0; i < count; i++) {
        int64_t key = 0;
        if (suit_cbor_get_int(&c, &key) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (key == SUIT_COSE_HDR_ALG) {
            return suit_cbor_get_int(&c, alg) == ESP_OK ? ESP_OK : ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (suit_cbor_skip(&c) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

/*
 * The unprotected header of a COSE_Encrypt: the IV, and for AES-CTR the algorithm too, because
 * the draft puts a zero-length protected header on an AES-CTR structure.
 */
static esp_err_t content_header(suit_cbor_t *c, int64_t *alg, uint8_t *iv, size_t *iv_len)
{
    size_t count = 0;
    if (suit_cbor_get_map(c, &count) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    for (size_t i = 0; i < count; i++) {
        int64_t key = 0;
        if (suit_cbor_get_int(c, &key) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (key == SUIT_COSE_HDR_ALG) {
            if (suit_cbor_get_int(c, alg) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
        } else if (key == SUIT_COSE_HDR_IV) {
            const uint8_t *p = NULL;
            size_t len = 0;
            if (suit_cbor_get_bstr(c, &p, &len) != ESP_OK || len > SUIT_IV_MAX) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            memcpy(iv, p, len);
            *iv_len = len;
        } else if (suit_cbor_skip(c) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
    }
    return *iv_len > 0 ? ESP_OK : ESP_ERR_SUIT_OTA_FORMAT;
}

/* the "salt" header parameter, label -20, of a serialized header map; absent is not an error */
static esp_err_t header_salt(const uint8_t *prot, size_t prot_len,
                             const uint8_t **salt, size_t *salt_len)
{
    if (prot_len == 0) {
        return ESP_OK;
    }
    suit_cbor_t c;
    suit_cbor_init(&c, prot, prot_len);
    size_t count = 0;
    if (suit_cbor_get_map(&c, &count) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    for (size_t i = 0; i < count; i++) {
        int64_t key = 0;
        if (suit_cbor_get_int(&c, &key) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (key == SUIT_COSE_HDR_SALT) {
            return suit_cbor_get_bstr(&c, salt, salt_len) == ESP_OK ? ESP_OK : ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (suit_cbor_skip(&c) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
    }
    return ESP_OK;
}

/*
 * The unprotected header of a recipient: the ephemeral sender key, label -1, as a P-256 point,
 * and the salt, label -20, when the author put it here rather than in the protected header.
 */
static esp_err_t ephemeral_key(suit_cbor_t *c, uint8_t point[SUIT_P256_POINT_LEN],
                               const uint8_t **salt, size_t *salt_len)
{
    size_t count = 0;
    if (suit_cbor_get_map(c, &count) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        int64_t key = 0;
        if (suit_cbor_get_int(c, &key) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (key == SUIT_COSE_HDR_SALT) {
            if (suit_cbor_get_bstr(c, salt, salt_len) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            continue;
        }
        if (key != SUIT_COSE_HDR_EPHEMERAL) {
            if (suit_cbor_skip(c) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            continue;
        }
        size_t members = 0;
        if (suit_cbor_get_map(c, &members) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        int64_t kty = 0, crv = 0;
        const uint8_t *x = NULL, *y = NULL;
        size_t x_len = 0, y_len = 0;
        for (size_t m = 0; m < members; m++) {
            int64_t label = 0;
            if (suit_cbor_get_int(c, &label) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            esp_err_t err;
            switch (label) {
            case SUIT_COSE_KEY_KTY: err = suit_cbor_get_int(c, &kty); break;
            case SUIT_COSE_KEY_CRV: err = suit_cbor_get_int(c, &crv); break;
            case SUIT_COSE_KEY_X:   err = suit_cbor_get_bstr(c, &x, &x_len); break;
            case SUIT_COSE_KEY_Y:   err = suit_cbor_get_bstr(c, &y, &y_len); break;
            default:                err = suit_cbor_skip(c); break;
            }
            if (err != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
        }
        if (kty != SUIT_COSE_KTY_EC2 || crv != SUIT_COSE_CRV_P256) {
            ESP_LOGE(TAG, "the ephemeral key is not an EC2 key on P-256");
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        }
        if (x_len != SUIT_P256_SCALAR_LEN || y_len != SUIT_P256_SCALAR_LEN) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        point[0] = SUIT_P256_POINT_UNCOMPRESSED;
        memcpy(point + 1, x, x_len);
        memcpy(point + 1 + x_len, y, y_len);
        found = true;
    }
    return found ? ESP_OK : ESP_ERR_SUIT_OTA_FORMAT;
}

/* ---------- key agreement and key unwrap ---------- */

static esp_err_t agree(struct esp_suit_ota_ctx *ctx, const uint8_t peer[SUIT_P256_POINT_LEN],
                       uint8_t out[SUIT_P256_SCALAR_LEN])
{
    if (ctx->cfg.decrypt_agree != NULL) {
        return ctx->cfg.decrypt_agree(ctx->cfg.decrypt_user, peer, out);
    }
    if (ctx->cfg.decrypt_key == NULL) {
        ESP_LOGE(TAG, "this payload is encrypted: set cfg.decrypt_key or cfg.decrypt_agree");
        return ESP_ERR_SUIT_OTA_DECRYPT;
    }
    if (psa_crypto_init() != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    psa_key_attributes_t attr = psa_key_attributes_init();
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&attr, PSA_ALG_ECDH);
    psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attr, SUIT_P256_KEY_BITS);
    psa_key_id_t key = 0;
    if (psa_import_key(&attr, ctx->cfg.decrypt_key, SUIT_P256_SCALAR_LEN, &key) != PSA_SUCCESS) {
        ESP_LOGE(TAG, "cfg.decrypt_key is not a P-256 private scalar");
        return ESP_ERR_SUIT_OTA_DECRYPT;
    }
    size_t olen = 0;
    psa_status_t st = psa_raw_key_agreement(PSA_ALG_ECDH, key, peer, SUIT_P256_POINT_LEN,
                                            out, SUIT_P256_SCALAR_LEN, &olen);
    psa_destroy_key(key);
    if (st != PSA_SUCCESS || olen != SUIT_P256_SCALAR_LEN) {
        ESP_LOGE(TAG, "key agreement with the ephemeral key failed");
        return ESP_ERR_SUIT_OTA_DECRYPT;
    }
    return ESP_OK;
}

/*
 * Draft section 5.2.4: a salt is optional, but when one arrives in the "salt" header parameter
 * the receiver must process it and pass it into the key derivation. RFC 9053 gives that
 * parameter the label -20. Without this, a release that salts its derivation would never unwrap.
 */
static esp_err_t hkdf(const uint8_t *ikm, size_t ikm_len, const uint8_t *salt, size_t salt_len,
                      const uint8_t *info, size_t info_len, uint8_t *out, size_t out_len)
{
    psa_key_derivation_operation_t op = psa_key_derivation_operation_init();
    psa_status_t st = psa_key_derivation_setup(&op, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    if (st == PSA_SUCCESS) {
        st = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SALT, salt, salt_len);
    }
    if (st == PSA_SUCCESS) {
        st = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SECRET, ikm, ikm_len);
    }
    if (st == PSA_SUCCESS) {
        st = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO, info, info_len);
    }
    if (st == PSA_SUCCESS) {
        st = psa_key_derivation_output_bytes(&op, out, out_len);
    }
    psa_key_derivation_abort(&op);
    return st == PSA_SUCCESS ? ESP_OK : ESP_FAIL;
}

/* one COSE_recipient: ECDH-ES with AES key wrap, giving the content key */
static esp_err_t recipient_key(struct esp_suit_ota_ctx *ctx, suit_cbor_t *rec,
                               uint8_t *cek, size_t *cek_len)
{
    size_t items = 0;
    if (suit_cbor_get_array(rec, &items) != ESP_OK || items != SUIT_COSE_RECIPIENT_ITEMS) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    const uint8_t *prot = NULL;
    size_t prot_len = 0;
    if (suit_cbor_get_bstr(rec, &prot, &prot_len) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    int64_t esdh_alg = 0;
    if (header_alg(prot, prot_len, &esdh_alg) != ESP_OK) {
        ESP_LOGE(TAG, "the recipient does not state its algorithm");
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    int64_t kw_alg;
    if (esdh_alg == SUIT_COSE_ALG_ECDH_ES_A128KW) {
        kw_alg = SUIT_COSE_ALG_A128KW;
    } else if (esdh_alg == SUIT_COSE_ALG_ECDH_ES_A256KW) {
        kw_alg = SUIT_COSE_ALG_A256KW;
    } else {
        ESP_LOGE(TAG, "recipient algorithm %lld is not ECDH-ES with AES key wrap", (long long)esdh_alg);
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
    /* draft section 11: the recipient algorithm must be one this device is configured to accept */
    if (!suit_alg_allowed(ctx->cfg.kex_algs, ctx->cfg.num_kex_algs, esdh_alg)) {
        ESP_LOGE(TAG, "key exchange algorithm %lld is not in this device's policy", (long long)esdh_alg);
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
    const uint8_t *salt = NULL;
    size_t salt_len = 0;
    if (header_salt(prot, prot_len, &salt, &salt_len) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    uint8_t peer[SUIT_P256_POINT_LEN];
    esp_err_t err = ephemeral_key(rec, peer, &salt, &salt_len);
    if (err != ESP_OK) {
        return err;
    }
    const uint8_t *wrapped = NULL;
    size_t wrapped_len = 0;
    if (suit_cbor_get_bstr(rec, &wrapped, &wrapped_len) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }

    uint8_t shared[SUIT_P256_SCALAR_LEN];
    err = agree(ctx, peer, shared);
    if (err == ESP_OK) {
        uint8_t info[SUIT_KDF_CONTEXT_MAX];
        size_t info_len = 0;
        err = kdf_context(kw_alg, prot, prot_len, info, sizeof(info), &info_len);
        if (err == ESP_OK) {
            uint8_t kek[SUIT_CEK_MAX];
            size_t kek_len = suit_kw_key_bytes(kw_alg);
            err = hkdf(shared, sizeof(shared), salt, salt_len, info, info_len, kek, kek_len);
            if (err == ESP_OK) {
                *cek_len = SUIT_CEK_MAX;
                err = suit_aes_key_unwrap(kek, kek_len, wrapped, wrapped_len, cek, cek_len);
            }
            memset(kek, 0, sizeof(kek));
        }
    }
    memset(shared, 0, sizeof(shared));
    return err;
}

/* ---------- the interface the rest of the component uses ---------- */

esp_err_t suit_decrypt_init(struct esp_suit_ota_ctx *ctx, size_t index,
                            const uint8_t *info, size_t info_len)
{
    suit_component_t *c = &ctx->comp[index];
    suit_decrypt_t *d = &c->decrypt;
    memset(d, 0, sizeof(*d));

    suit_cbor_t cb;
    suit_cbor_init(&cb, info, info_len);
    uint64_t tag = 0;
    if (suit_cbor_get_tag(&cb, &tag) != ESP_OK || tag != SUIT_COSE_ENCRYPT_TAG) {
        ESP_LOGE(TAG, "encryption info is not #6.96(COSE_Encrypt)");
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    size_t items = 0;
    if (suit_cbor_get_array(&cb, &items) != ESP_OK || items != SUIT_COSE_ENCRYPT_ITEMS) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    const uint8_t *prot = NULL;
    size_t prot_len = 0;
    if (suit_cbor_get_bstr(&cb, &prot, &prot_len) != ESP_OK || prot_len > sizeof(d->prot)) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    d->prot_len = prot_len;
    memcpy(d->prot, prot, prot_len);
    header_alg(prot, prot_len, &d->alg);               /* AES-GCM states its algorithm here */
    esp_err_t err = content_header(&cb, &d->alg, d->iv, &d->iv_len);
    if (err != ESP_OK) {
        return err;
    }
    if (suit_cbor_get_nil(&cb) != ESP_OK) {
        ESP_LOGE(TAG, "the ciphertext of an encrypted payload is detached, so the field is nil");
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    size_t recipients = 0;
    if (suit_cbor_get_array(&cb, &recipients) != ESP_OK || recipients == 0) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }

    switch (d->alg) {
    case SUIT_COSE_ALG_A128GCM:
    case SUIT_COSE_ALG_A256GCM:
        d->aead = true;
        break;
    case SUIT_COSE_ALG_A128CTR:
    case SUIT_COSE_ALG_A256CTR:
        d->aead = false;
        break;
    default:
        ESP_LOGE(TAG, "content algorithm %lld is not supported", (long long)d->alg);
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
    /* draft section 11: reject an algorithm outside this device's policy, before any key work */
    if (!suit_alg_allowed(ctx->cfg.content_algs, ctx->cfg.num_content_algs, d->alg)) {
        ESP_LOGE(TAG, "content algorithm %lld is not in this device's policy", (long long)d->alg);
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
    if (d->iv_len != (d->aead ? SUIT_GCM_IV_LEN : SUIT_CTR_IV_LEN)) {
        ESP_LOGE(TAG, "the IV is %u bytes, which does not fit the content algorithm",
                 (unsigned)d->iv_len);
        return ESP_ERR_SUIT_OTA_FORMAT;
    }

    /*
     * Try the recipients in order and keep the first that unwraps. A release addressed to more
     * than one device carries one recipient each, and only ours yields a key.
     */
    err = ESP_ERR_SUIT_OTA_DECRYPT;
    for (size_t i = 0; i < recipients; i++) {
        suit_cbor_t rec = cb;
        esp_err_t one = recipient_key(ctx, &rec, d->cek, &d->cek_len);
        if (one == ESP_OK) {
            err = ESP_OK;
            break;
        }
        if (one != ESP_ERR_SUIT_OTA_DECRYPT) {
            return one;                                /* malformed, or no key is configured */
        }
        if (suit_cbor_skip(&cb) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no recipient of this release is for this device");
        return err;
    }
    if (d->cek_len != suit_content_key_bytes(d->alg)) {
        ESP_LOGE(TAG, "the wrapped key is %u bytes, which does not fit the content algorithm",
                 (unsigned)d->cek_len);
        return ESP_ERR_SUIT_OTA_DECRYPT;
    }
    d->ready = true;
    ESP_LOGI(TAG, "%s: encrypted payload, the content key was unwrapped for this device", c->id);
    return ESP_OK;
}

esp_err_t suit_decrypt_start(suit_decrypt_t *d)
{
    if (!d->ready) {
        return ESP_ERR_INVALID_STATE;
    }
    psa_key_attributes_t attr = psa_key_attributes_init();
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attr, d->aead ? PSA_ALG_GCM : PSA_ALG_CTR);
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attr, d->cek_len * 8);
    if (psa_import_key(&attr, d->cek, d->cek_len, &d->key) != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    psa_status_t st;
    if (d->aead) {
        d->aead_op = psa_aead_operation_init();
        st = psa_aead_decrypt_setup(&d->aead_op, d->key, PSA_ALG_GCM);
        if (st == PSA_SUCCESS) {
            st = psa_aead_set_nonce(&d->aead_op, d->iv, d->iv_len);
        }
        if (st == PSA_SUCCESS) {
            uint8_t aad[SUIT_ENC_STRUCTURE_MAX];
            size_t aad_len = 0;
            st = (enc_structure(d->prot, d->prot_len, aad, sizeof(aad), &aad_len) == ESP_OK)
                 ? psa_aead_update_ad(&d->aead_op, aad, aad_len) : PSA_ERROR_NOT_SUPPORTED;
        }
    } else {
        d->cipher_op = psa_cipher_operation_init();
        st = psa_cipher_decrypt_setup(&d->cipher_op, d->key, PSA_ALG_CTR);
        if (st == PSA_SUCCESS) {
            st = psa_cipher_set_iv(&d->cipher_op, d->iv, d->iv_len);
        }
    }
    if (st != PSA_SUCCESS) {
        psa_destroy_key(d->key);
        d->key = 0;
        return ESP_FAIL;
    }
    d->tail_len = 0;
    d->active = true;
    return ESP_OK;
}

/*
 * Decrypt one chunk and hand the plaintext to sink.
 *
 * An AEAD holds the last SUIT_GCM_TAG_LEN bytes of the resource back, because they are the tag
 * and not payload. Keeping them in a small buffer means the tag never reaches the flash or the
 * digest, and is still there to verify at the end.
 */
esp_err_t suit_decrypt_write(suit_decrypt_t *d, const uint8_t *data, size_t len,
                             suit_decrypt_sink_fn sink, void *user)
{
    if (!d->active) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t plain[SUIT_DECRYPT_CHUNK];
    while (len > 0) {
        size_t olen = 0;
        esp_err_t err;
        if (d->aead) {
            uint8_t joined[SUIT_GCM_TAG_LEN + SUIT_DECRYPT_CHUNK];
            size_t have = d->tail_len;
            memcpy(joined, d->tail, have);
            size_t room = sizeof(joined) - have;
            size_t copy = len < room ? len : room;
            memcpy(joined + have, data, copy);
            have += copy;
            data += copy;
            len -= copy;
            size_t keep = have < SUIT_GCM_TAG_LEN ? have : SUIT_GCM_TAG_LEN;
            size_t give = have - keep;
            memcpy(d->tail, joined + give, keep);
            d->tail_len = keep;
            if (give == 0) {
                continue;
            }
            if (psa_aead_update(&d->aead_op, joined, give, plain, sizeof(plain), &olen) != PSA_SUCCESS) {
                return ESP_ERR_SUIT_OTA_DECRYPT;
            }
        } else {
            size_t take = len < sizeof(plain) ? len : sizeof(plain);
            if (psa_cipher_update(&d->cipher_op, data, take, plain, sizeof(plain), &olen) != PSA_SUCCESS) {
                return ESP_ERR_SUIT_OTA_DECRYPT;
            }
            data += take;
            len -= take;
        }
        err = (olen > 0) ? sink(user, plain, olen) : ESP_OK;
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t suit_decrypt_finish(suit_decrypt_t *d, suit_decrypt_sink_fn sink, void *user)
{
    if (!d->active) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t plain[SUIT_DECRYPT_CHUNK];
    size_t olen = 0;
    esp_err_t err = ESP_OK;
    if (d->aead) {
        if (d->tail_len != SUIT_GCM_TAG_LEN) {
            ESP_LOGE(TAG, "the payload ended before its authentication tag");
            err = ESP_ERR_SUIT_OTA_INCOMPLETE;
        } else if (psa_aead_verify(&d->aead_op, plain, sizeof(plain), &olen,
                                   d->tail, d->tail_len) != PSA_SUCCESS) {
            ESP_LOGE(TAG, "the authentication tag does not match: the payload was changed");
            err = ESP_ERR_SUIT_OTA_DECRYPT;
        } else if (olen > 0) {
            err = sink(user, plain, olen);
        }
    } else {
        if (psa_cipher_finish(&d->cipher_op, plain, sizeof(plain), &olen) != PSA_SUCCESS) {
            err = ESP_ERR_SUIT_OTA_DECRYPT;
        } else if (olen > 0) {
            err = sink(user, plain, olen);
        }
    }
    d->active = false;                                 /* the operation is finished either way */
    suit_decrypt_abort(d);
    return err;
}

void suit_decrypt_abort(suit_decrypt_t *d)
{
    if (d->active) {
        if (d->aead) {
            psa_aead_abort(&d->aead_op);
        } else {
            psa_cipher_abort(&d->cipher_op);
        }
        d->active = false;
    }
    if (d->key != 0) {
        psa_destroy_key(d->key);
        d->key = 0;
    }
    memset(d->cek, 0, sizeof(d->cek));
    d->tail_len = 0;
}

#endif /* CONFIG_ESP_SUIT_OTA_ENCRYPTED_PAYLOAD */
