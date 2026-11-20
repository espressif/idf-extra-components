/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "suit_priv.h"

static const char *TAG = SUIT_TAG;

typedef enum {
    VAL_SKIP = 0,
    VAL_AUTH,
    VAL_MANIFEST,
    VAL_SEVERED,
    VAL_TEXT,
    VAL_PAYLOAD,
} value_mode_t;

/* Accumulate bytes into fr->head until a full item head decodes. 1 = done, 0 = need more, -1 = bad. */
static int read_head(suit_frame_t *fr, const uint8_t **data, size_t *len, suit_cbor_head_t *h)
{
    while (*len > 0) {
        if (fr->head_len >= sizeof(fr->head)) {
            return -1;
        }
        fr->head[fr->head_len++] = **data;
        (*data)++;
        (*len)--;
        int r = suit_cbor_head(fr->head, fr->head_len, h);
        if (r < 0) {
            return -1;
        }
        if (r > 0) {
            fr->head_len = 0;
            return 1;
        }
    }
    return 0;
}

static size_t arena_free(struct esp_suit_ota_ctx *ctx)
{
    return ctx->arena_size - ctx->arena_used;
}

/* one envelope entry consumed; the last one ends the envelope at once, without waiting for a byte */
static esp_err_t entry_done(struct esp_suit_ota_ctx *ctx)
{
    suit_frame_t *fr = &ctx->fr;
    fr->entries_left--;
    fr->state = SUIT_FR_KEY;
    if (fr->entries_left == 0) {
        fr->state = SUIT_FR_DONE;
        return suit_after_envelope(ctx);
    }
    return ESP_OK;
}

static esp_err_t begin_collect(struct esp_suit_ota_ctx *ctx, const suit_cbor_head_t *h, bool keep_head, bool hash)
{
    suit_frame_t *fr = &ctx->fr;
    size_t need = (size_t)h->val + (keep_head ? h->len : 0);
    if (h->val > SIZE_MAX / 2 || need > arena_free(ctx)) {
        ESP_LOGE(TAG, "envelope entry of %llu bytes does not fit the arena", (unsigned long long)h->val);
        return ESP_ERR_SUIT_OTA_SIZE;
    }
    fr->collect_off = ctx->arena_used;
    fr->collect_head_len = 0;
    if (hash) {
        esp_err_t err = suit_hash_start(&fr->hash);
        if (err != ESP_OK) {
            return err;
        }
    }
    if (keep_head) {
        uint8_t head[SUIT_CBOR_HEAD_MAX];
        size_t n = 0;
        uint64_t v = h->val;
        head[n++] = SUIT_CBOR_INITIAL(SUIT_CBOR_BSTR, h->ai);
        for (int i = h->len - 2; i >= 0; i--) {
            head[n++] = (uint8_t)(v >> (8 * i));
        }
        memcpy(ctx->arena + ctx->arena_used, head, n);
        ctx->arena_used += n;
        fr->collect_head_len = n;
        if (hash) {
            suit_hash_update(&fr->hash, head, n);
        }
    }
    fr->remaining = h->val;
    fr->state = SUIT_FR_COLLECT;
    return ESP_OK;
}

static esp_err_t begin_hash_skip(struct esp_suit_ota_ctx *ctx, const suit_cbor_head_t *h)
{
    suit_frame_t *fr = &ctx->fr;
    esp_err_t err = suit_hash_start(&fr->hash);
    if (err != ESP_OK) {
        return err;
    }
    uint8_t head[SUIT_CBOR_HEAD_MAX];
    size_t n = 0;
    head[n++] = SUIT_CBOR_INITIAL(SUIT_CBOR_BSTR, h->ai);
    for (int i = h->len - 2; i >= 0; i--) {
        head[n++] = (uint8_t)(h->val >> (8 * i));
    }
    suit_hash_update(&fr->hash, head, n);
    fr->remaining = h->val;
    fr->state = SUIT_FR_HASH_SKIP;
    return ESP_OK;
}

static esp_err_t finish_collect(struct esp_suit_ota_ctx *ctx)
{
    suit_frame_t *fr = &ctx->fr;
    const uint8_t *start = ctx->arena + fr->collect_off;
    size_t total = ctx->arena_used - fr->collect_off;
    esp_err_t err = ESP_OK;
    switch (fr->key) {
    case SUIT_ENV_AUTH:
        ctx->auth.ptr = start;
        ctx->auth.len = total;
        fr->auth_seen = true;
        break;
    case SUIT_ENV_MANIFEST:
        ctx->manifest_bstr.ptr = start;
        ctx->manifest_bstr.len = total;
        fr->manifest_seen = true;
        err = suit_on_manifest_complete(ctx);
        break;
    case SUIT_ENV_PAYLOAD_FETCH:
    case SUIT_ENV_INSTALL: {
        uint8_t digest[SUIT_DIGEST_LEN];
        err = suit_hash_finish(&fr->hash, digest);
        if (err == ESP_OK) {
            err = suit_on_severed_section(ctx, (int)fr->key, start, total, digest);
        }
        break;
    }
    case SUIT_ENV_TEXT: {
        uint8_t digest[SUIT_DIGEST_LEN];
        err = suit_hash_finish(&fr->hash, digest);
        if (err == ESP_OK) {
            err = suit_on_text_bytes(ctx, start + fr->collect_head_len, total - fr->collect_head_len, digest);
        }
        ctx->arena_used = fr->collect_off;      /* text is not kept */
        break;
    }
    default:
        break;
    }
    if (err != ESP_OK) {
        return err;
    }
    return entry_done(ctx);
}

static esp_err_t on_value_head(struct esp_suit_ota_ctx *ctx, const suit_cbor_head_t *h, value_mode_t mode)
{
    suit_frame_t *fr = &ctx->fr;
    switch (mode) {
    case VAL_SKIP:
        fr->skip_items = 1;
        fr->remaining = 0;
        fr->state = SUIT_FR_SKIP;
        /* re-dispatch this head through the skipper */
        if (h->major == SUIT_CBOR_BSTR || h->major == SUIT_CBOR_TSTR) {
            fr->remaining = h->val;
            fr->skip_items = 0;
        } else if (h->major == SUIT_CBOR_ARRAY) {
            fr->skip_items = h->val;
        } else if (h->major == SUIT_CBOR_MAP) {
            fr->skip_items = SUIT_CBOR_ITEMS_PER_MAP_ENTRY * h->val;
        } else if (h->major == SUIT_CBOR_TAG) {
            fr->skip_items = 1;
        } else {
            fr->skip_items = 0;
        }
        return ESP_OK;
    case VAL_AUTH:
        if (h->major != SUIT_CBOR_BSTR) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (h->val > CONFIG_ESP_SUIT_OTA_AUTH_MAX) {
            ESP_LOGE(TAG, "authentication wrapper of %llu bytes exceeds the cap", (unsigned long long)h->val);
            return ESP_ERR_SUIT_OTA_AUTH;
        }
        return begin_collect(ctx, h, false, false);
    case VAL_MANIFEST:
        if (h->major != SUIT_CBOR_BSTR) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        return begin_collect(ctx, h, true, false);
    case VAL_SEVERED:
        if (h->major != SUIT_CBOR_BSTR) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        return begin_collect(ctx, h, true, true);
    case VAL_TEXT:
        if (h->major != SUIT_CBOR_BSTR) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (h->val + h->len <= arena_free(ctx)) {
            return begin_collect(ctx, h, true, true);
        }
        return begin_hash_skip(ctx, h);
    case VAL_PAYLOAD: {
        if (h->major != SUIT_CBOR_BSTR) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        int ci = suit_find_integrated_component(ctx, fr->tkey, fr->tkey_len);
        if (ci < 0) {
            ESP_LOGW(TAG, "integrated payload '%.*s' is not needed, skipped", (int)fr->tkey_len, fr->tkey);
            fr->remaining = h->val;
            fr->skip_items = 0;
            fr->state = SUIT_FR_SKIP;
            return ESP_OK;
        }
        suit_component_t *c = &ctx->comp[ci];
        if (h->val != c->size) {
            ESP_LOGE(TAG, "integrated payload '%s' is %llu bytes, manifest says %lu", c->id,
                     (unsigned long long)h->val, (unsigned long)c->size);
            return ESP_ERR_SUIT_OTA_SIZE;
        }
        esp_err_t err = suit_stream_open(ctx, (size_t)ci, 0);
        if (err != ESP_OK) {
            return err;
        }
        fr->payload_comp = ci;
        fr->remaining = h->val;
        fr->state = SUIT_FR_PAYLOAD;
        if (h->val == 0) {
            err = suit_payload_boundary(ctx);
            if (err != ESP_OK) {
                return err;
            }
            return entry_done(ctx);
        }
        return ESP_OK;
    }
    }
    return ESP_ERR_SUIT_OTA_FORMAT;
}

static esp_err_t on_key(struct esp_suit_ota_ctx *ctx, const suit_cbor_head_t *h, const uint8_t **data, size_t *len)
{
    suit_frame_t *fr = &ctx->fr;
    if (h->major == SUIT_CBOR_UINT) {
        if (fr->tkey_len > 0 || (int64_t)h->val <= fr->last_int_key) {
            ESP_LOGE(TAG, "envelope keys are not in canonical order");
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        fr->key = (int64_t)h->val;
        fr->last_int_key = fr->key;
        value_mode_t mode;
        switch (fr->key) {
        case SUIT_ENV_DELEGATION:
            mode = VAL_SKIP;
            break;
        case SUIT_ENV_AUTH:
            mode = VAL_AUTH;
            break;
        case SUIT_ENV_MANIFEST:
            if (!fr->auth_seen) {
                ESP_LOGE(TAG, "manifest before the authentication wrapper");
                return ESP_ERR_SUIT_OTA_AUTH;
            }
            mode = VAL_MANIFEST;
            break;
        case SUIT_ENV_PAYLOAD_FETCH:
        case SUIT_ENV_INSTALL: {
            if (!fr->manifest_seen) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            suit_section_t *s = (fr->key == SUIT_ENV_INSTALL) ? &ctx->man.install : &ctx->man.payload_fetch;
            if (!s->present || !s->severed) {
                ESP_LOGE(TAG, "envelope carries section %lld that the manifest did not sever", (long long)fr->key);
                return ESP_ERR_SUIT_OTA_SEVERED;
            }
            mode = VAL_SEVERED;
            break;
        }
        case SUIT_ENV_TEXT:
            if (!fr->manifest_seen) {
                return ESP_ERR_SUIT_OTA_FORMAT;
            }
            mode = (ctx->man.text.present && ctx->man.text.severed) ? VAL_TEXT : VAL_SKIP;
            break;
        default:
            mode = VAL_SKIP;
            break;
        }
        fr->state = SUIT_FR_VALUE_HEAD;
        fr->remaining = mode;           /* mode rides in remaining until the value head is read */
        return ESP_OK;
    }
    if (h->major == SUIT_CBOR_TSTR) {
        if (!fr->manifest_seen) {
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
        if (h->val == 0 || h->val >= sizeof(fr->tkey)) {
            ESP_LOGE(TAG, "integrated payload key longer than %u bytes", (unsigned)sizeof(fr->tkey) - 1);
            return ESP_ERR_SUIT_OTA_UNSUPPORTED;
        }
        fr->key = -1;
        fr->tkey_len = 0;
        fr->remaining = h->val;
        fr->state = SUIT_FR_TSTR_KEY;
        return ESP_OK;
    }
    return ESP_ERR_SUIT_OTA_FORMAT;
}

static esp_err_t skip_step(struct esp_suit_ota_ctx *ctx, const uint8_t **data, size_t *len)
{
    suit_frame_t *fr = &ctx->fr;
    if (fr->remaining > 0) {
        size_t n = *len < fr->remaining ? *len : (size_t)fr->remaining;
        *data += n;
        *len -= n;
        fr->remaining -= n;
        if (fr->remaining > 0 || fr->skip_items > 0) {
            return ESP_OK;
        }
    }
    if (fr->skip_items == 0) {
        return entry_done(ctx);
    }
    suit_cbor_head_t h;
    int r = read_head(fr, data, len, &h);
    if (r < 0) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    if (r == 0) {
        return ESP_OK;
    }
    fr->skip_items--;
    switch (h.major) {
    case SUIT_CBOR_BSTR:
    case SUIT_CBOR_TSTR:
        fr->remaining = h.val;
        break;
    case SUIT_CBOR_ARRAY:
        fr->skip_items += h.val;
        break;
    case SUIT_CBOR_MAP:
        fr->skip_items += SUIT_CBOR_ITEMS_PER_MAP_ENTRY * h.val;
        break;
    case SUIT_CBOR_TAG:
        fr->skip_items += 1;
        break;
    default:
        break;
    }
    if (fr->skip_items > SUIT_FRAME_SKIP_ITEMS_MAX) {
        return ESP_ERR_SUIT_OTA_FORMAT;
    }
    return ESP_OK;
}

esp_err_t suit_frame_feed(struct esp_suit_ota_ctx *ctx, const uint8_t *data, size_t len, size_t *used)
{
    suit_frame_t *fr = &ctx->fr;
    const uint8_t *p = data;
    size_t left = len;
    esp_err_t err = ESP_OK;

    if (len == 0) {
        /* the current resource ended */
        switch (fr->state) {
        case SUIT_FR_URL: {
            suit_component_t *c = &ctx->comp[ctx->current];
            ESP_LOGE(TAG, "%s: resource ended after %lu of %lu bytes", c->id, (unsigned long)c->bytes_written,
                     (unsigned long)c->size);
            return ESP_ERR_SUIT_OTA_INCOMPLETE;
        }
        case SUIT_FR_READY:
            return ESP_SUIT_OTA_READY;
        case SUIT_FR_DONE:
            return ESP_OK;
        default:
            ESP_LOGE(TAG, "envelope ended early");
            return ESP_ERR_SUIT_OTA_INCOMPLETE;
        }
    }

    while (left > 0 && err == ESP_OK) {
        suit_cbor_head_t h;
        int r;
        switch (fr->state) {
        case SUIT_FR_START:
            r = read_head(fr, &p, &left, &h);
            if (r < 0) {
                err = ESP_ERR_SUIT_OTA_FORMAT;
            } else if (r > 0) {
                if (h.major == SUIT_CBOR_TAG) {
                    if (h.val != SUIT_ENVELOPE_TAG || fr->tag_seen) {
                        err = ESP_ERR_SUIT_OTA_FORMAT;
                    }
                    fr->tag_seen = true;
                } else if (h.major == SUIT_CBOR_MAP && h.val >= SUIT_ENVELOPE_MIN_ENTRIES) {
                    fr->entries_left = (size_t)h.val;
                    fr->last_int_key = -1;
                    fr->state = SUIT_FR_KEY;
                } else {
                    ESP_LOGE(TAG, "not a SUIT envelope");
                    err = ESP_ERR_SUIT_OTA_FORMAT;
                }
            }
            break;
        case SUIT_FR_KEY:
            if (fr->entries_left == 0) {
                fr->state = SUIT_FR_DONE;
                err = suit_after_envelope(ctx);
                break;
            }
            r = read_head(fr, &p, &left, &h);
            if (r < 0) {
                err = ESP_ERR_SUIT_OTA_FORMAT;
            } else if (r > 0) {
                err = on_key(ctx, &h, &p, &left);
            }
            break;
        case SUIT_FR_TSTR_KEY: {
            size_t n = left < fr->remaining ? left : (size_t)fr->remaining;
            memcpy(fr->tkey + fr->tkey_len, p, n);
            fr->tkey_len += n;
            p += n;
            left -= n;
            fr->remaining -= n;
            if (fr->remaining == 0) {
                fr->tkey[fr->tkey_len] = '\0';
                fr->state = SUIT_FR_VALUE_HEAD;
                fr->remaining = VAL_PAYLOAD;
            }
            break;
        }
        case SUIT_FR_VALUE_HEAD: {
            value_mode_t mode = (value_mode_t)fr->remaining;
            r = read_head(fr, &p, &left, &h);
            if (r < 0) {
                err = ESP_ERR_SUIT_OTA_FORMAT;
            } else if (r > 0) {
                err = on_value_head(ctx, &h, mode);
                if (err == ESP_OK && fr->state == SUIT_FR_COLLECT && fr->remaining == 0) {
                    err = finish_collect(ctx);
                } else if (err == ESP_OK && fr->state == SUIT_FR_SKIP && fr->remaining == 0 && fr->skip_items == 0) {
                    err = entry_done(ctx);
                } else if (err == ESP_OK && fr->state == SUIT_FR_HASH_SKIP && fr->remaining == 0) {
                    uint8_t digest[SUIT_DIGEST_LEN];
                    suit_hash_finish(&fr->hash, digest);
                    err = suit_on_text_bytes(ctx, NULL, 0, digest);
                    if (err == ESP_OK) {
                        err = entry_done(ctx);
                    }
                }
            }
            break;
        }
        case SUIT_FR_COLLECT: {
            size_t n = left < fr->remaining ? left : (size_t)fr->remaining;
            memcpy(ctx->arena + ctx->arena_used, p, n);
            if (fr->hash.active) {
                suit_hash_update(&fr->hash, p, n);
            }
            ctx->arena_used += n;
            p += n;
            left -= n;
            fr->remaining -= n;
            if (fr->remaining == 0) {
                err = finish_collect(ctx);
            }
            break;
        }
        case SUIT_FR_HASH_SKIP: {
            size_t n = left < fr->remaining ? left : (size_t)fr->remaining;
            suit_hash_update(&fr->hash, p, n);
            p += n;
            left -= n;
            fr->remaining -= n;
            if (fr->remaining == 0) {
                uint8_t digest[SUIT_DIGEST_LEN];
                suit_hash_finish(&fr->hash, digest);
                err = suit_on_text_bytes(ctx, NULL, 0, digest);
                if (err == ESP_OK) {
                    err = entry_done(ctx);
                }
            }
            break;
        }
        case SUIT_FR_SKIP:
            err = skip_step(ctx, &p, &left);
            break;
        case SUIT_FR_PAYLOAD: {
            size_t n = left < fr->remaining ? left : (size_t)fr->remaining;
            err = suit_payload_write(ctx, p, n);
            p += n;
            left -= n;
            fr->remaining -= n;
            if (err == ESP_OK && fr->remaining == 0) {
                err = suit_payload_boundary(ctx);
                if (err == ESP_OK) {
                    err = entry_done(ctx);
                }
            }
            break;
        }
        case SUIT_FR_URL: {
            suit_component_t *c = &ctx->comp[ctx->current];
            size_t room = c->size - c->bytes_written;
            if (room == 0) {
                ESP_LOGE(TAG, "%s: resource is longer than %lu bytes", c->id, (unsigned long)c->size);
                err = ESP_ERR_SUIT_OTA_SIZE;
                break;
            }
            size_t n = left < room ? left : room;
            err = suit_payload_write(ctx, p, n);
            p += n;
            left -= n;
            if (err == ESP_OK && c->bytes_written == c->size) {
                err = suit_payload_boundary(ctx);
                if (err == ESP_OK) {
                    if (left > 0) {
                        ESP_LOGE(TAG, "%s: resource is longer than %lu bytes", c->id, (unsigned long)c->size);
                        err = ESP_ERR_SUIT_OTA_SIZE;
                    } else {
                        err = suit_after_envelope(ctx);   /* next URL, or READY */
                    }
                }
            }
            break;
        }
        case SUIT_FR_DONE:
            ESP_LOGE(TAG, "bytes after the end of the envelope");
            err = ESP_ERR_SUIT_OTA_FORMAT;
            break;
        case SUIT_FR_READY:
            ESP_LOGE(TAG, "bytes after the campaign became ready");
            err = ESP_ERR_SUIT_OTA_FORMAT;
            break;
        case SUIT_FR_FAILED:
            err = ctx->sticky_err;
            break;
        default:
            err = ESP_ERR_SUIT_OTA_STATE;
            break;
        }
        if (err == ESP_SUIT_OTA_READY) {
            break;
        }
    }
    *used = len - left;
    if (err == ESP_OK && fr->state == SUIT_FR_READY) {
        return ESP_SUIT_OTA_READY;
    }
    if (err == ESP_SUIT_OTA_READY) {
        fr->state = SUIT_FR_READY;
        if (left > 0) {
            ESP_LOGE(TAG, "bytes after the campaign became ready");
            return ESP_ERR_SUIT_OTA_FORMAT;
        }
    }
    return err;
}
