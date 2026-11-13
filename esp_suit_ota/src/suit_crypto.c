/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "suit_priv.h"
#include "psa/crypto.h"

static esp_err_t ensure_psa(void)
{
    psa_status_t st = psa_crypto_init();
    return st == PSA_SUCCESS ? ESP_OK : ESP_FAIL;
}

esp_err_t suit_hash_start(suit_hash_t *h)
{
    if (ensure_psa() != ESP_OK) {
        return ESP_FAIL;
    }
    psa_hash_operation_t *op = &h->op;
    *op = psa_hash_operation_init();
    if (psa_hash_setup(op, PSA_ALG_SHA_256) != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    h->active = true;
    return ESP_OK;
}

esp_err_t suit_hash_update(suit_hash_t *h, const void *data, size_t len)
{
    if (!h->active) {
        return ESP_ERR_INVALID_STATE;
    }
    if (len == 0) {
        return ESP_OK;
    }
    if ((uintptr_t)data % sizeof(uint32_t) == 0) {
        return psa_hash_update(&h->op, data, len) == PSA_SUCCESS ? ESP_OK : ESP_FAIL;
    }
    /*
     * The SHA peripheral drivers read their input one 32-bit word at a time. On some chips an
     * unaligned word load from flash-mapped memory (an envelope embedded in the app or mmapped from
     * a partition) is a load access fault, so unaligned input goes through an aligned copy.
     */
    uint32_t bounce[SUIT_SHA256_BLOCK_LEN / sizeof(uint32_t)];     /* one SHA-256 block, word aligned */
    const uint8_t *p = data;
    while (len > 0) {
        size_t n = len < sizeof(bounce) ? len : sizeof(bounce);
        memcpy(bounce, p, n);
        if (psa_hash_update(&h->op, (const uint8_t *)bounce, n) != PSA_SUCCESS) {
            return ESP_FAIL;
        }
        p += n;
        len -= n;
    }
    return ESP_OK;
}

esp_err_t suit_hash_finish(suit_hash_t *h, uint8_t out[SUIT_DIGEST_LEN])
{
    if (!h->active) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t olen = 0;
    psa_status_t st = psa_hash_finish(&h->op, out, SUIT_DIGEST_LEN, &olen);
    h->active = false;
    return (st == PSA_SUCCESS && olen == SUIT_DIGEST_LEN) ? ESP_OK : ESP_FAIL;
}

void suit_hash_abort(suit_hash_t *h)
{
    if (h->active) {
        psa_hash_abort(&h->op);
        h->active = false;
    }
}

esp_err_t suit_sha256(const void *data, size_t len, uint8_t out[SUIT_DIGEST_LEN])
{
    suit_hash_t h = { 0 };
    esp_err_t err = suit_hash_start(&h);
    if (err == ESP_OK) {
        err = suit_hash_update(&h, data, len);      /* takes care of unaligned input */
    }
    if (err == ESP_OK) {
        return suit_hash_finish(&h, out);
    }
    suit_hash_abort(&h);
    return err;
}

esp_err_t suit_ecdsa_p256_verify(const uint8_t *point, size_t point_len, const uint8_t hash[SUIT_DIGEST_LEN],
                                 const uint8_t sig[SUIT_P256_SIG_LEN])
{
    if (point == NULL || point_len != SUIT_P256_POINT_LEN || point[0] != SUIT_P256_POINT_UNCOMPRESSED) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ensure_psa() != ESP_OK) {
        return ESP_FAIL;
    }
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attr, SUIT_P256_KEY_BITS);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_VERIFY_HASH);
    psa_set_key_algorithm(&attr, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
    psa_key_id_t key = 0;
    psa_status_t st = psa_import_key(&attr, point, point_len, &key);
    psa_reset_key_attributes(&attr);
    if (st != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    st = psa_verify_hash(key, PSA_ALG_ECDSA(PSA_ALG_SHA_256), hash, SUIT_DIGEST_LEN, sig, SUIT_P256_SIG_LEN);
    psa_destroy_key(key);
    if (st == PSA_SUCCESS) {
        return ESP_OK;
    }
    return st == PSA_ERROR_INVALID_SIGNATURE ? ESP_ERR_SUIT_OTA_AUTH : ESP_FAIL;
}
