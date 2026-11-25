/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include "suit_priv.h"
#include "esp_suit_ota_helpers.h"
#include "esp_http_client.h"

static const char *TAG = SUIT_TAG;

#define MAX_REDIRECTS 5
#define RUN_BUF_SIZE  4096
#define RANGE_HDR_MAX 48      /* "bytes=<start>-<end>" with two 32-bit numbers */
#define DRAIN_BUF_LEN 128     /* body of a redirect reply, read and dropped */

typedef struct {
    esp_http_client_handle_t client;
    const esp_suit_ota_https_cfg_t *cfg;
    uint32_t epoch;
    bool open;
    bool at_end;
    size_t delivered;          /* bytes handed to the core for this resource */
    size_t chunk_left;         /* bytes left in the current Range chunk, SIZE_MAX when not chunked */
    size_t skip;               /* bytes to discard: a 200 reply to a Range request */
    char etag[sizeof(((esp_suit_ota_transport_t *)0)->manifest_etag)];
    bool etag_stored;          /* an ETag from the installed record was sent as If-None-Match */
    http_event_handle_cb user_event_cb;
} https_state_t;

static esp_err_t event_cb(esp_http_client_event_t *evt)
{
    https_state_t *st = evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_HEADER && st != NULL && strcasecmp(evt->header_key, "ETag") == 0) {
        strlcpy(st->etag, evt->header_value, sizeof(st->etag));
    }
    if (st != NULL && st->user_event_cb != NULL) {
        return st->user_event_cb(evt);
    }
    return ESP_OK;
}

static void close_client(https_state_t *st)
{
    if (st->client != NULL) {
        esp_http_client_close(st->client);
        esp_http_client_cleanup(st->client);
        st->client = NULL;
    }
    st->open = false;
}

static esp_err_t open_resource(esp_suit_ota_transport_t *t, https_state_t *st, size_t start)
{
    const esp_suit_ota_https_cfg_t *cfg = st->cfg;
    esp_http_client_config_t hc = *cfg->http_config;
    hc.url = t->url;
    hc.event_handler = event_cb;
    hc.user_data = st;
    st->user_event_cb = cfg->http_config->event_handler;
#if !CONFIG_ESP_SUIT_OTA_HTTPS_ALLOW_HTTP
    if (hc.cert_pem == NULL && !hc.use_global_ca_store && hc.crt_bundle_attach == NULL) {
        ESP_LOGE(TAG, "server verification is required: set cert_pem, crt_bundle_attach or use_global_ca_store");
        return ESP_ERR_INVALID_ARG;
    }
    hc.transport_type = HTTP_TRANSPORT_OVER_SSL;
#endif
    close_client(st);
    st->client = esp_http_client_init(&hc);
    if (st->client == NULL) {
        return ESP_FAIL;
    }
    if (cfg->init_cb != NULL) {
        esp_err_t err = cfg->init_cb(st->client);
        if (err != ESP_OK) {
            close_client(st);
            return err;
        }
    }
    size_t end = SIZE_MAX;
    if (cfg->max_request_size > 0 && t->len != ESP_SUIT_OTA_LEN_UNKNOWN) {
        end = start + cfg->max_request_size - 1;
        size_t last = t->offset + t->len - 1;
        if (end > last) {
            end = last;
        }
    }
    char range[RANGE_HDR_MAX];
    if (end != SIZE_MAX) {
        snprintf(range, sizeof(range), "bytes=%u-%u", (unsigned)start, (unsigned)end);
        esp_http_client_set_header(st->client, "Range", range);
    } else if (start > 0) {
        snprintf(range, sizeof(range), "bytes=%u-", (unsigned)start);
        esp_http_client_set_header(st->client, "Range", range);
    }
    st->etag_stored = false;
    if (t->allow_not_modified) {
        char stored[sizeof(st->etag)];
        if (suit_persist_etag_read(stored, sizeof(stored)) == ESP_OK && stored[0] != '\0') {
            esp_http_client_set_header(st->client, "If-None-Match", stored);
            st->etag_stored = true;
        }
    }
    st->etag[0] = '\0';
    esp_err_t err = ESP_OK;
    int status = 0;
    for (int redirects = 0; ; redirects++) {
        err = esp_http_client_open(st->client, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "open %s failed: %s", t->url, esp_err_to_name(err));
            close_client(st);
            return err;
        }
        int64_t content_len = esp_http_client_fetch_headers(st->client);
        if (content_len < 0) {
            ESP_LOGE(TAG, "no response headers from %s", t->url);
            close_client(st);
            return ESP_FAIL;
        }
        status = esp_http_client_get_status_code(st->client);
        if (status >= HttpStatus_MultipleChoices && status < HttpStatus_BadRequest && status != HttpStatus_NotModified) {
            if (redirects >= MAX_REDIRECTS) {
                close_client(st);
                return ESP_FAIL;
            }
            esp_http_client_set_redirection(st->client);
            char drain[DRAIN_BUF_LEN];
            while (esp_http_client_read(st->client, drain, sizeof(drain)) > 0) {
            }
            esp_http_client_close(st->client);
            continue;
        }
        break;
    }
    if (status == HttpStatus_NotModified && t->allow_not_modified && st->etag_stored) {
        close_client(st);
        return ESP_ERR_SUIT_OTA_NOT_MODIFIED;
    }
    if (status == HttpStatus_RangeNotSatisfiable) {
        ESP_LOGE(TAG, "range %u- is not satisfiable", (unsigned)start);
        close_client(st);
        return ESP_ERR_SUIT_OTA_INCOMPLETE;
    }
    if (status != HttpStatus_Ok && status != HttpStatus_PartialContent) {
        ESP_LOGE(TAG, "%s answered %d", t->url, status);
        close_client(st);
        return ESP_FAIL;
    }
    st->skip = (status == HttpStatus_Ok && start > 0) ? start : 0;   /* the server ignored the Range header */
    if (status == HttpStatus_Ok && end != SIZE_MAX) {
        end = SIZE_MAX;                                      /* whole resource instead of a chunk */
    }
    st->chunk_left = (end == SIZE_MAX) ? SIZE_MAX : end - start + 1;
    st->open = true;
    st->at_end = false;
    if (t->allow_not_modified) {
        strlcpy(t->manifest_etag, st->etag, sizeof(t->manifest_etag));
    }
    suit_post_event(ESP_SUIT_OTA_EVENT_CONNECTED, NULL, 0);
    return ESP_OK;
}

int esp_suit_ota_https_get(esp_suit_ota_transport_t *t, void **state, uint8_t *buf, size_t cap)
{
    https_state_t *st = *state;
    if (st == NULL) {
        st = calloc(1, sizeof(*st));
        if (st == NULL) {
            return -ESP_ERR_NO_MEM;
        }
        st->cfg = t->https.cfg;
        *state = st;
    }
    if (st->cfg == NULL || st->cfg->http_config == NULL) {
        return -ESP_ERR_INVALID_ARG;
    }
    if (!st->open || st->epoch != t->epoch) {
        st->epoch = t->epoch;
        st->delivered = 0;
        esp_err_t err = open_resource(t, st, t->offset);
        if (err != ESP_OK) {
            return -err;
        }
    }
    if (t->len != ESP_SUIT_OTA_LEN_UNKNOWN && st->delivered >= t->len) {
        return 0;
    }
    for (;;) {
        if (st->at_end) {
            return 0;
        }
        if (st->chunk_left == 0 && t->len != ESP_SUIT_OTA_LEN_UNKNOWN && st->delivered < t->len) {
            esp_err_t err = open_resource(t, st, t->offset + st->delivered);   /* next Range chunk */
            if (err != ESP_OK) {
                return -err;
            }
            continue;
        }
        size_t want = cap;
        if (t->len != ESP_SUIT_OTA_LEN_UNKNOWN && want > t->len - st->delivered) {
            want = t->len - st->delivered;
        }
        if (st->chunk_left != SIZE_MAX && want > st->chunk_left) {
            want = st->chunk_left;
        }
        if (st->skip > 0) {
            size_t n = st->skip < cap ? st->skip : cap;
            int r = esp_http_client_read(st->client, (char *)buf, n);
            if (r < 0) {
                return -ESP_FAIL;
            }
            if (r == 0) {
                return -ESP_ERR_SUIT_OTA_INCOMPLETE;
            }
            st->skip -= (size_t)r;
            continue;
        }
        if (want == 0) {
            return 0;
        }
        int r = esp_http_client_read(st->client, (char *)buf, want);
        if (r < 0) {
            ESP_LOGE(TAG, "read failed: %d", r);
            return -ESP_FAIL;
        }
        if (r == 0) {
            if (st->chunk_left != SIZE_MAX && st->chunk_left > 0) {
                return -ESP_ERR_SUIT_OTA_INCOMPLETE;        /* the chunk ended early */
            }
            if (st->chunk_left == 0 && t->len != ESP_SUIT_OTA_LEN_UNKNOWN && st->delivered < t->len) {
                esp_err_t err = open_resource(t, st, t->offset + st->delivered);   /* next chunk */
                if (err != ESP_OK) {
                    return -err;
                }
                continue;
            }
            st->at_end = true;
            return 0;
        }
        st->delivered += (size_t)r;
        if (st->chunk_left != SIZE_MAX) {
            st->chunk_left -= (size_t)r;
        }
        return r;
    }
}

void esp_suit_ota_https_close(void *state)
{
    https_state_t *st = state;
    if (st == NULL) {
        return;
    }
    close_client(st);
    free(st);
}
