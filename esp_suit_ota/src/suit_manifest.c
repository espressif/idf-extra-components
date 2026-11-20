/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "suit_priv.h"

static const char *TAG = SUIT_TAG;

static esp_err_t parse_section(suit_cbor_t *c, suit_section_t *s)
{
    suit_cbor_head_t h;
    esp_err_t err = suit_cbor_peek(c, &h);
    if (err != ESP_OK) {
        return err;
    }
    s->present = true;
    if (h.major == SUIT_CBOR_BSTR) {
        s->severed = false;
        s->have_bytes = true;
        return suit_cbor_get_bstr(c, &s->bytes.ptr, &s->bytes.len);
    }
    if (h.major != SUIT_CBOR_ARRAY) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    size_t n;
    int64_t alg;
    const uint8_t *d;
    size_t dlen;
    if (suit_cbor_get_array(c, &n) != ESP_OK || n != SUIT_DIGEST_ITEMS || suit_cbor_get_int(c, &alg) != ESP_OK ||
            suit_cbor_get_bstr(c, &d, &dlen) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    if (alg != SUIT_DIGEST_ALG_SHA256) {
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
    if (dlen != SUIT_DIGEST_LEN) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    s->severed = true;
    s->have_bytes = false;
    memcpy(s->digest, d, SUIT_DIGEST_LEN);
    return ESP_OK;
}

static esp_err_t parse_components(suit_cbor_t *c, suit_manifest_t *m)
{
    size_t n;
    esp_err_t err = suit_cbor_get_array(c, &n);
    if (err != ESP_OK) {
        return err;
    }
    if (n == 0) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    if (n > SUIT_MAX_COMP) {
        ESP_LOGE(TAG, "%u components, limit is %d", (unsigned)n, SUIT_MAX_COMP);
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
    m->num_components = n;
    for (size_t i = 0; i < n; i++) {
        size_t parts;
        if (suit_cbor_get_array(c, &parts) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (parts == 0 || parts > ESP_SUIT_OTA_CID_MAX_PARTS) {
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        }
        m->cid_num_parts[i] = parts;
        for (size_t p = 0; p < parts; p++) {
            const uint8_t *s;
            size_t slen;
            if (suit_cbor_get_bstr(c, &s, &slen) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            m->cid_parts[i][p].ptr = s;
            m->cid_parts[i][p].len = slen;
        }
    }
    for (size_t i = 0; i < n; i++) {
        for (size_t j = i + 1; j < n; j++) {
            if (suit_cid_equal(m->cid_parts[i], m->cid_num_parts[i], m->cid_parts[j], m->cid_num_parts[j])) {
                ESP_LOGE(TAG, "duplicate component identifier");
                return ESP_ERR_SUIT_OTA_MAP;
            }
        }
    }
    return ESP_OK;
}

static esp_err_t parse_common(const uint8_t *p, size_t len, suit_manifest_t *m)
{
    suit_cbor_t c;
    suit_cbor_init(&c, p, len);
    size_t n;
    esp_err_t err = suit_cbor_get_map(&c, &n);
    if (err != ESP_OK) {
        return err;
    }
    int64_t last = -1;
    for (size_t i = 0; i < n; i++) {
        int64_t k;
        if (suit_cbor_get_int(&c, &k) != ESP_OK || k <= last) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        last = k;
        switch (k) {
        case SUIT_COMMON_COMPONENTS:
            err = parse_components(&c, m);
            break;
        case SUIT_COMMON_SHARED_SEQ:
            err = suit_cbor_get_bstr(&c, &m->shared_seq.ptr, &m->shared_seq.len);
            break;
        case SUIT_COMMON_DEPENDENCIES:
            ESP_LOGE(TAG, "manifest dependencies (trust domains) are not supported");
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        default:
            ESP_LOGE(TAG, "unknown common key %lld", (long long)k);
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        }
        if (err != ESP_OK) {
            return err;
        }
    }
    if (!suit_cbor_at_end(&c)) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    if (m->num_components == 0) {
        ESP_LOGE(TAG, "manifest declares no components");
        return ESP_ERR_SUIT_OTA_UNSUPPORTED;
    }
    return ESP_OK;
}

esp_err_t suit_manifest_parse(const uint8_t *manifest, size_t len, suit_manifest_t *m)
{
    memset(m, 0, sizeof(*m));
    suit_cbor_t c;
    suit_cbor_init(&c, manifest, len);
    size_t n;
    esp_err_t err = suit_cbor_get_map(&c, &n);
    if (err != ESP_OK) {
        return err;
    }
    bool have_version = false, have_seq = false, have_common = false;
    int64_t last = -1;
    const uint8_t *common = NULL;
    size_t common_len = 0;
    for (size_t i = 0; i < n; i++) {
        int64_t k;
        if (suit_cbor_get_int(&c, &k) != ESP_OK || k <= last) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        last = k;
        uint64_t u;
        switch (k) {
        case SUIT_MAN_VERSION:
            if (suit_cbor_get_uint(&c, &u) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            if (u != SUIT_MANIFEST_VERSION) {
                ESP_LOGE(TAG, "manifest version %llu is not supported", (unsigned long long)u);
                return ESP_ERR_SUIT_OTA_UNSUPPORTED;
            }
            have_version = true;
            break;
        case SUIT_MAN_SEQUENCE:
            if (suit_cbor_get_uint(&c, &m->sequence_number) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            have_seq = true;
            break;
        case SUIT_MAN_COMMON:
            if (suit_cbor_get_bstr(&c, &common, &common_len) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            have_common = true;
            break;
        case SUIT_MAN_VALIDATE:
            if (suit_cbor_get_bstr(&c, &m->validate.ptr, &m->validate.len) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            break;
        case SUIT_MAN_LOAD:
        case SUIT_MAN_INVOKE: {
            /* invocation procedure: parsed for shape only, never run */
            const uint8_t *s;
            size_t slen;
            if (suit_cbor_get_bstr(&c, &s, &slen) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            suit_cbor_t sc;
            suit_cbor_init(&sc, s, slen);
            size_t cnt;
            if (suit_cbor_get_array(&sc, &cnt) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            break;
        }
        case SUIT_MAN_PAYLOAD_FETCH:
            err = parse_section(&c, &m->payload_fetch);
            break;
        case SUIT_MAN_INSTALL:
            err = parse_section(&c, &m->install);
            break;
        case SUIT_MAN_TEXT:
            err = parse_section(&c, &m->text);
            if (err == ESP_OK && !m->text.severed) {
                m->text_inline = m->text.bytes;
            }
            break;
        case SUIT_MAN_REFERENCE_URI:   /* the URI of the full manifest: this profile never fetches it */
        case SUIT_MAN_COMPONENT_ID:
        case SUIT_MAN_COSWID:
        case SUIT_MAN_UNINSTALL:
            err = suit_cbor_skip(&c);
            break;
        case SUIT_MAN_DEP_RESOLUTION:
        case SUIT_MAN_CANDIDATE_VERIFICATION:
            ESP_LOGE(TAG, "manifest key %lld (trust domains) is not supported", (long long)k);
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        default:
            ESP_LOGE(TAG, "unknown manifest key %lld", (long long)k);
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        }
        if (err != ESP_OK) {
            return err;
        }
    }
    if (!suit_cbor_at_end(&c)) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    if (!have_version || !have_seq || !have_common) {
        ESP_LOGE(TAG, "manifest lacks version, sequence number or common section");
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    return parse_common(common, common_len, m);
}

esp_err_t suit_text_description(const uint8_t *text_map, size_t len, const char *lang, char *out, size_t cap)
{
    suit_cbor_t c;
    suit_cbor_init(&c, text_map, len);
    size_t n;
    if (suit_cbor_get_map(&c, &n) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    size_t lang_len = strlen(lang);
    const uint8_t *chosen = NULL;
    size_t chosen_len = 0;
    for (size_t i = 0; i < n; i++) {
        const uint8_t *tag;
        size_t tag_len;
        const uint8_t *lmap;
        size_t lmap_len;
        if (suit_cbor_get_tstr(&c, &tag, &tag_len) != ESP_OK || suit_cbor_get_raw(&c, &lmap, &lmap_len) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        bool match = (tag_len == lang_len && memcmp(tag, lang, lang_len) == 0);
        if (chosen == NULL || match) {
            chosen = lmap;
            chosen_len = lmap_len;
        }
        if (match) {
            break;
        }
    }
    if (chosen == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    suit_cbor_init(&c, chosen, chosen_len);
    if (suit_cbor_get_map(&c, &n) != ESP_OK) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    for (size_t i = 0; i < n; i++) {
        suit_cbor_head_t h;
        if (suit_cbor_peek(&c, &h) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (h.major == SUIT_CBOR_UINT && h.val == SUIT_TEXT_UPDATE_DESCRIPTION) {
            c.p += h.len;
            const uint8_t *s;
            size_t slen;
            if (suit_cbor_get_tstr(&c, &s, &slen) != ESP_OK) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            size_t ncpy = slen < cap - 1 ? slen : cap - 1;
            memcpy(out, s, ncpy);
            out[ncpy] = '\0';
            return ESP_OK;
        }
        if (suit_cbor_skip(&c) != ESP_OK || suit_cbor_skip(&c) != ESP_OK) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
    }
    return ESP_ERR_NOT_FOUND;
}
