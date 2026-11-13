/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "suit_cbor.h"
#include "esp_suit_ota_err.h"

int suit_cbor_head(const uint8_t *p, size_t n, suit_cbor_head_t *h)
{
    if (n < 1) {
        return 0;
    }
    uint8_t ib = p[0];
    h->major = ib >> SUIT_CBOR_MAJOR_SHIFT;
    h->ai = ib & SUIT_CBOR_AI_MASK;
    if (h->ai <= SUIT_CBOR_AI_DIRECT_MAX) {
        h->val = h->ai;
        h->len = 1;
        return 1;
    }
    size_t need;
    switch (h->ai) {
    case SUIT_CBOR_AI_1BYTE: need = sizeof(uint8_t); break;
    case SUIT_CBOR_AI_2BYTE: need = sizeof(uint16_t); break;
    case SUIT_CBOR_AI_4BYTE: need = sizeof(uint32_t); break;
    case SUIT_CBOR_AI_8BYTE: need = sizeof(uint64_t); break;
    default: return -1;             /* 28..30 reserved, 31 indefinite */
    }
    if (n < 1 + need) {
        return 0;
    }
    uint64_t v = 0;
    for (size_t i = 0; i < need; i++) {
        v = (v << 8) | p[1 + i];
    }
    if (h->major == SUIT_CBOR_SIMPLE) {
        if (h->ai != SUIT_CBOR_AI_1BYTE || v < SUIT_CBOR_SIMPLE_1BYTE_MIN) {
            return -1;              /* floats are not part of the profile; simple values 0..31 use ai directly */
        }
    } else {
        /* shortest form: the value must not fit a smaller argument */
        uint64_t floor = (need == sizeof(uint8_t)) ? SUIT_CBOR_1BYTE_MIN : (need == sizeof(uint16_t)) ? SUIT_CBOR_2BYTE_MIN :
                         (need == sizeof(uint32_t)) ? SUIT_CBOR_4BYTE_MIN : SUIT_CBOR_8BYTE_MIN;
        if (v < floor) {
            return -1;
        }
    }
    h->val = v;
    h->len = (uint8_t)(1 + need);
    return h->len;
}

esp_err_t suit_cbor_peek(const suit_cbor_t *c, suit_cbor_head_t *h)
{
    int r = suit_cbor_head(c->p, (size_t)(c->end - c->p), h);
    return r > 0 ? ESP_OK : ESP_ERR_SUIT_OTA_FORMAT;
}

static esp_err_t take_head(suit_cbor_t *c, uint8_t major, suit_cbor_head_t *h)
{
    esp_err_t err = suit_cbor_peek(c, h);
    if (err != ESP_OK) {
        return err;
    }
    if (h->major != major) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    c->p += h->len;
    return ESP_OK;
}

esp_err_t suit_cbor_get_uint(suit_cbor_t *c, uint64_t *v)
{
    suit_cbor_head_t h;
    esp_err_t err = take_head(c, SUIT_CBOR_UINT, &h);
    if (err == ESP_OK) {
        *v = h.val;
    }
    return err;
}

esp_err_t suit_cbor_get_int(suit_cbor_t *c, int64_t *v)
{
    suit_cbor_head_t h;
    esp_err_t err = suit_cbor_peek(c, &h);
    if (err != ESP_OK) {
        return err;
    }
    if (h.major == SUIT_CBOR_UINT) {
        if (h.val > INT64_MAX) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        *v = (int64_t)h.val;
    } else if (h.major == SUIT_CBOR_NINT) {
        if (h.val > INT64_MAX) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        *v = -1 - (int64_t)h.val;
    } else {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    c->p += h.len;
    return ESP_OK;
}

static esp_err_t get_string(suit_cbor_t *c, uint8_t major, const uint8_t **ptr, size_t *len)
{
    suit_cbor_head_t h;
    esp_err_t err = take_head(c, major, &h);
    if (err != ESP_OK) {
        return err;
    }
    if (h.val > (uint64_t)(c->end - c->p)) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    *ptr = c->p;
    *len = (size_t)h.val;
    c->p += h.val;
    return ESP_OK;
}

esp_err_t suit_cbor_get_bstr(suit_cbor_t *c, const uint8_t **ptr, size_t *len)
{
    return get_string(c, SUIT_CBOR_BSTR, ptr, len);
}

esp_err_t suit_cbor_get_tstr(suit_cbor_t *c, const uint8_t **ptr, size_t *len)
{
    return get_string(c, SUIT_CBOR_TSTR, ptr, len);
}

esp_err_t suit_cbor_get_bool(suit_cbor_t *c, bool *v)
{
    suit_cbor_head_t h;
    esp_err_t err = suit_cbor_peek(c, &h);
    if (err != ESP_OK) {
        return err;
    }
    if (h.major != SUIT_CBOR_SIMPLE || h.len != 1 || (h.val != SUIT_CBOR_SIMPLE_FALSE && h.val != SUIT_CBOR_SIMPLE_TRUE)) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    *v = (h.val == SUIT_CBOR_SIMPLE_TRUE);
    c->p += 1;
    return ESP_OK;
}

esp_err_t suit_cbor_get_nil(suit_cbor_t *c)
{
    suit_cbor_head_t h;
    esp_err_t err = suit_cbor_peek(c, &h);
    if (err != ESP_OK) {
        return err;
    }
    if (h.major != SUIT_CBOR_SIMPLE || h.len != 1 || h.val != SUIT_CBOR_SIMPLE_NULL) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    c->p += 1;
    return ESP_OK;
}

esp_err_t suit_cbor_get_array(suit_cbor_t *c, size_t *count)
{
    suit_cbor_head_t h;
    esp_err_t err = take_head(c, SUIT_CBOR_ARRAY, &h);
    if (err == ESP_OK) {
        if (h.val > (uint64_t)(c->end - c->p)) {
            return ESP_ERR_SUIT_OTA_FORMAT;   /* every item takes at least one byte */
        }
        *count = (size_t)h.val;
    }
    return err;
}

esp_err_t suit_cbor_get_map(suit_cbor_t *c, size_t *count)
{
    suit_cbor_head_t h;
    esp_err_t err = take_head(c, SUIT_CBOR_MAP, &h);
    if (err == ESP_OK) {
        if (h.val > (uint64_t)(c->end - c->p) / SUIT_CBOR_ITEMS_PER_MAP_ENTRY) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        *count = (size_t)h.val;
    }
    return err;
}

esp_err_t suit_cbor_get_tag(suit_cbor_t *c, uint64_t *tag)
{
    suit_cbor_head_t h;
    esp_err_t err = take_head(c, SUIT_CBOR_TAG, &h);
    if (err == ESP_OK) {
        *tag = h.val;
    }
    return err;
}

esp_err_t suit_cbor_skip(suit_cbor_t *c)
{
    uint64_t pending = 1;
    while (pending > 0) {
        suit_cbor_head_t h;
        esp_err_t err = suit_cbor_peek(c, &h);
        if (err != ESP_OK) {
            return err;
        }
        c->p += h.len;
        pending--;
        switch (h.major) {
        case SUIT_CBOR_BSTR:
        case SUIT_CBOR_TSTR:
            if (h.val > (uint64_t)(c->end - c->p)) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            c->p += h.val;
            break;
        case SUIT_CBOR_ARRAY:
            pending += h.val;
            break;
        case SUIT_CBOR_MAP:
            pending += SUIT_CBOR_ITEMS_PER_MAP_ENTRY * h.val;
            break;
        case SUIT_CBOR_TAG:
            pending += 1;
            break;
        default:
            break;
        }
        if (pending > (uint64_t)(c->end - c->p) + 1) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
    }
    return ESP_OK;
}

esp_err_t suit_cbor_get_raw(suit_cbor_t *c, const uint8_t **ptr, size_t *len)
{
    const uint8_t *start = c->p;
    esp_err_t err = suit_cbor_skip(c);
    if (err != ESP_OK) {
        return err;
    }
    *ptr = start;
    *len = (size_t)(c->p - start);
    return ESP_OK;
}
