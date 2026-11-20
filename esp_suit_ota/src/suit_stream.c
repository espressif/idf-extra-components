/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include "suit_priv.h"

static const char *TAG = SUIT_TAG;

#define READBACK_CHUNK 1024

static bool needs_header_probe(const suit_component_t *c)
{
    return c->target.sink == NULL && c->target.verify == ESP_SUIT_VERIFY_APP_IMAGE;
}

static esp_err_t sink_open(suit_component_t *c, size_t resume_offset)
{
    esp_err_t err;
    if (c->target.sink != NULL) {
        err = c->target.sink->open(c->target.sink->ctx, c->size);
    } else {
        err = suit_sink_part_open(c, resume_offset);
    }
    if (err == ESP_OK) {
        c->sink_open = true;
    }
    return err;
}

static esp_err_t sink_write(suit_component_t *c, const uint8_t *data, size_t len)
{
    if (c->target.sink != NULL) {
        return c->target.sink->write(c->target.sink->ctx, c->bytes_written, data, len);
    }
    return suit_sink_part_write(c, data, len);
}

/* hashed, then written, with the app header probed first */
static esp_err_t plain_write(struct esp_suit_ota_ctx *ctx, suit_component_t *c, const uint8_t *data, size_t len)
{
    esp_err_t err = suit_hash_update(&ctx->stream_hash, data, len);
    if (err != ESP_OK) {
        return err;
    }
    if (!c->header_checked && needs_header_probe(c)) {
        size_t room = SUIT_HEADER_PROBE_LEN - c->header_len;
        size_t take = len < room ? len : room;
        memcpy(c->header + c->header_len, data, take);
        c->header_len += take;
        c->bytes_written += take;
        if (c->header_len < SUIT_HEADER_PROBE_LEN && c->bytes_written < c->size) {
            return ESP_OK;              /* keep collecting */
        }
        err = suit_sink_check_header(c);
        if (err != ESP_OK) {
            return err;
        }
        c->header_checked = true;
        c->bytes_written -= c->header_len;
        if (!c->sink_open) {
            err = sink_open(c, 0);
            if (err != ESP_OK) {
                return err;
            }
        }
        err = sink_write(c, c->header, c->header_len);
        if (err != ESP_OK) {
            return err;
        }
        c->bytes_written += c->header_len;
        data += take;
        len -= take;
        if (len == 0) {
            return ESP_OK;
        }
    }
    if (!c->sink_open) {
        err = sink_open(c, 0);
        if (err != ESP_OK) {
            return err;
        }
    }
    err = sink_write(c, data, len);
    if (err != ESP_OK) {
        return err;
    }
    c->bytes_written += len;
    return ESP_OK;
}

esp_err_t suit_stream_open(struct esp_suit_ota_ctx *ctx, size_t index, size_t resume_offset)
{
    suit_component_t *c = &ctx->comp[index];
    esp_err_t err = suit_hash_start(&ctx->stream_hash);
    if (err != ESP_OK) {
        return err;
    }
    c->bytes_written = 0;
    c->header_len = 0;
    c->header_checked = false;
    c->sink_open = false;
    c->state = SUIT_COMP_WRITING;
    ctx->current = index;
    if (resume_offset == 0) {
        return ESP_OK;
    }
    /* rebuild the digest state from the bytes already in the staging partition */
    if (c->target.staging == NULL) {
        return ESP_OK;                  /* a sink cannot be read back: start over */
    }
    uint8_t *buf = malloc(READBACK_CHUNK);
    if (buf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    size_t off = 0;
    while (off < resume_offset) {
        size_t n = resume_offset - off < READBACK_CHUNK ? resume_offset - off : READBACK_CHUNK;
        err = suit_sink_part_read(c->target.staging, off, buf, n);
        if (err != ESP_OK) {
            break;
        }
        err = suit_hash_update(&ctx->stream_hash, buf, n);
        if (err != ESP_OK) {
            break;
        }
        if (needs_header_probe(c) && c->header_len < SUIT_HEADER_PROBE_LEN) {
            size_t take = SUIT_HEADER_PROBE_LEN - c->header_len;
            if (take > n) {
                take = n;
            }
            memcpy(c->header + c->header_len, buf, take);
            c->header_len += take;
        }
        off += n;
    }
    free(buf);
    if (err != ESP_OK) {
        return err;
    }
    if (needs_header_probe(c)) {
        err = suit_sink_check_header(c);
        if (err != ESP_OK) {
            return err;
        }
        c->header_checked = true;
    }
    err = sink_open(c, resume_offset);
    if (err != ESP_OK) {
        return err;
    }
    c->bytes_written = resume_offset;
    ctx->journal_mark = resume_offset;
    return ESP_OK;
}

esp_err_t suit_stream_write(struct esp_suit_ota_ctx *ctx, size_t index, const uint8_t *data, size_t len)
{
    return plain_write(ctx, &ctx->comp[index], data, len);
}

esp_err_t suit_stream_close(struct esp_suit_ota_ctx *ctx, size_t index)
{
    suit_component_t *c = &ctx->comp[index];
    uint8_t actual[SUIT_DIGEST_LEN];
    esp_err_t err = suit_hash_finish(&ctx->stream_hash, actual);
    if (err != ESP_OK) {
        return err;
    }
    if (c->bytes_written != c->size) {
        ESP_LOGE(TAG, "%s: %lu of %lu bytes", c->id, (unsigned long)c->bytes_written, (unsigned long)c->size);
        return ESP_ERR_SUIT_OTA_INCOMPLETE;
    }
    if (memcmp(actual, c->digest, SUIT_DIGEST_LEN) != 0) {
        ESP_LOGE(TAG, "%s: digest mismatch", c->id);
        return ESP_ERR_SUIT_OTA_DIGEST;
    }
    if (c->target.sink != NULL) {
        uint8_t remote[SUIT_DIGEST_LEN];
        err = c->target.sink->close(c->target.sink->ctx, remote);
        c->sink_open = false;
        if (err == ESP_OK && memcmp(remote, actual, SUIT_DIGEST_LEN) != 0) {
            ESP_LOGE(TAG, "%s: the sink computed a different digest", c->id);
            return ESP_ERR_SUIT_OTA_DIGEST;
        }
        if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
            return ESP_ERR_SUIT_OTA_IMAGE;
        }
    } else {
        if (!c->sink_open) {
            err = sink_open(c, 0);       /* zero-length payload */
            if (err != ESP_OK) {
                return err;
            }
        }
        err = suit_sink_part_close(c);
        c->sink_open = false;
        if (err != ESP_OK) {
            return err;
        }
    }
    c->state = SUIT_COMP_STAGED;
    ctx->current = SIZE_MAX;
    esp_suit_ota_event_component_t ev = { .index = (uint8_t)index, .size = c->size };
    strlcpy(ev.id, c->id, sizeof(ev.id));
    strlcpy(ev.uri, c->uri, sizeof(ev.uri));
    suit_post_event(ESP_SUIT_OTA_EVENT_COMPONENT_STAGED, &ev, sizeof(ev));
    ESP_LOGI(TAG, "%s staged and verified (%lu bytes)", c->id, (unsigned long)c->size);
    return ESP_OK;
}

esp_err_t suit_stream_abort(struct esp_suit_ota_ctx *ctx, size_t index)
{
    suit_component_t *c = &ctx->comp[index];
    suit_hash_abort(&ctx->stream_hash);
    if (c->sink_open) {
        if (c->target.sink != NULL) {
            if (c->target.sink->abort != NULL) {
                c->target.sink->abort(c->target.sink->ctx);
            }
        } else {
            suit_sink_part_abort(c);
        }
        c->sink_open = false;
    }
    if (c->state == SUIT_COMP_WRITING) {
        c->state = SUIT_COMP_PENDING;
    }
    ctx->current = SIZE_MAX;
    return ESP_OK;
}

esp_err_t suit_stream_digest_of_target(struct esp_suit_ota_ctx *ctx, size_t index, bool staging,
                                       uint8_t out[SUIT_DIGEST_LEN])
{
    suit_component_t *c = &ctx->comp[index];
    const esp_partition_t *p = staging ? c->target.staging : c->target.active;
    size_t size = c->size;
    if (c->target.sink != NULL) {
        if (!staging || c->target.sink->read == NULL) {
            return ESP_ERR_NOT_SUPPORTED;
        }
    } else if (p == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    } else if (size > p->size) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    uint8_t *buf = malloc(READBACK_CHUNK);
    if (buf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    suit_hash_t h;
    esp_err_t err = suit_hash_start(&h);
    size_t off = 0;
    while (err == ESP_OK && off < size) {
        size_t n = size - off < READBACK_CHUNK ? size - off : READBACK_CHUNK;
        if (c->target.sink != NULL) {
            err = c->target.sink->read(c->target.sink->ctx, off, buf, n);
        } else {
            err = suit_sink_part_read(p, off, buf, n);
        }
        if (err == ESP_OK) {
            err = suit_hash_update(&h, buf, n);
        }
        off += n;
    }
    if (err == ESP_OK) {
        err = suit_hash_finish(&h, out);
    } else {
        suit_hash_abort(&h);
    }
    free(buf);
    return err;
}
