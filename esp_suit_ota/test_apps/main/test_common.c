/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "unity.h"
#include "nvs.h"
#include "test_common.h"
#include "vectors.h"

extern const uint8_t compliance_start[] asm("_binary_compliance_bin_start");

/* ---------- memory sinks ---------- */

static uint8_t s_buf0[36 * 1024];      /* Appendix B Example 1 declares a 34768-byte payload */
static uint8_t s_buf1[4096];
static uint8_t s_buf2[4096];
mem_sink_t g_mem[3] = {
    { .buf = s_buf0, .cap = sizeof(s_buf0) },
    { .buf = s_buf1, .cap = sizeof(s_buf1) },
    { .buf = s_buf2, .cap = sizeof(s_buf2) },
};

static esp_err_t mem_open(void *ctx, size_t total)
{
    mem_sink_t *m = ctx;
    if (total > m->cap) {
        return ESP_ERR_INVALID_SIZE;
    }
    m->size = total;
    memset(m->buf, 0xff, m->cap);
    return ESP_OK;
}

static esp_err_t mem_write(void *ctx, size_t off, const void *data, size_t len)
{
    mem_sink_t *m = ctx;
    if (off + len > m->size) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(m->buf + off, data, len);
    return ESP_OK;
}

static esp_err_t mem_read(void *ctx, size_t off, void *data, size_t len)
{
    mem_sink_t *m = ctx;
    if (off + len > m->cap) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(data, m->buf + off, len);
    return ESP_OK;
}

static esp_err_t mem_close(void *ctx, uint8_t digest_out[32])
{
    ((mem_sink_t *)ctx)->closes++;
    return ESP_ERR_NOT_SUPPORTED;      /* the core's own digest is the only one */
}

static const esp_suit_ota_sink_ops_t s_sink[3] = {
    { mem_open, mem_write, mem_read, mem_close, NULL, &g_mem[0] },
    { mem_open, mem_write, mem_read, mem_close, NULL, &g_mem[1] },
    { mem_open, mem_write, mem_read, mem_close, NULL, &g_mem[2] },
};

/* ---------- the device under test ---------- */

static const uint8_t s_id[3] = { 0x00, 0x01, 0x02 };
static const esp_suit_ota_span_t s_cid[3][1] = { { { &s_id[0], 1 } }, { { &s_id[1], 1 } }, { { &s_id[2], 1 } } };
static const esp_suit_ota_map_entry_t s_map[3] = {
    { .id = { s_cid[0], 1 }, .verify = ESP_SUIT_VERIFY_DIGEST, .sink = &s_sink[0] },
    { .id = { s_cid[1], 1 }, .verify = ESP_SUIT_VERIFY_DIGEST, .sink = &s_sink[1] },
    { .id = { s_cid[2], 1 }, .verify = ESP_SUIT_VERIFY_DIGEST, .sink = &s_sink[2] },
};
static const uint8_t s_vendor[1][16] = {
    { 0xfa, 0x6b, 0x4a, 0x53, 0xd5, 0xad, 0x5f, 0xdf, 0xbe, 0x9d, 0xe6, 0x63, 0xe4, 0xd4, 0x1f, 0xfe },
};
static const uint8_t s_class[1][16] = {
    { 0x14, 0x92, 0xaf, 0x14, 0x25, 0x69, 0x5e, 0x48, 0xbf, 0x42, 0x9b, 0x2d, 0x51, 0xf2, 0xab, 0x45 },
};
static const esp_suit_ota_key_t s_key = { .point = suit_spec_pubkey, .point_len = 65 };

const esp_suit_ota_config_t g_cfg = {
    .keys = &s_key, .num_keys = 1,
    .vendor_ids = s_vendor, .num_vendor_ids = 1,
    .vendor_pens = &suit_cvec_pen, .num_vendor_pens = 1,
    .class_ids = s_class, .num_class_ids = 1,
    .device_id = suit_cvec_device_id,
    .map = s_map, .map_len = 3,
};

void test_reset_state(void)
{
    static const char *const namespaces[] = { "esp_suit", "ota_slots" };
    for (size_t i = 0; i < 2; i++) {
        nvs_handle_t h;
        if (nvs_open(namespaces[i], NVS_READWRITE, &h) == ESP_OK) {
            nvs_erase_all(h);
            nvs_commit(h);
            nvs_close(h);
        }
    }
    for (size_t i = 0; i < 3; i++) {
        memset(g_mem[i].buf, 0xff, g_mem[i].cap);
        g_mem[i].size = 0;
        g_mem[i].closes = 0;
    }
}

const suit_cvec_t *cvec_find(const char *name)
{
    for (size_t i = 0; i < suit_cvecs_count; i++) {
        if (strcmp(suit_cvecs[i].name, name) == 0) {
            return &suit_cvecs[i];
        }
    }
    TEST_FAIL_MESSAGE(name);
    return NULL;
}

const uint8_t *cvec_data(const suit_cvec_t *v)
{
    return compliance_start + v->offset;
}

/* ---------- a transport that serves the envelope and the generated payloads ---------- */

static const uint8_t *s_env;
static size_t s_env_len;

typedef struct {
    uint32_t epoch;
    size_t delivered;
    bool open;
} test_get_state_t;

static int test_get(esp_suit_ota_transport_t *t, void **state, uint8_t *buf, size_t cap)
{
    static test_get_state_t st;
    if (*state == NULL) {
        memset(&st, 0, sizeof(st));
        *state = &st;
    }
    test_get_state_t *s = *state;
    if (!s->open || s->epoch != t->epoch) {
        s->epoch = t->epoch;
        s->delivered = 0;
        s->open = true;
    }
    const uint8_t *data = NULL;
    size_t len = 0;
    if (strcmp(t->url, TEST_MANIFEST_URL) == 0) {
        data = s_env;
        len = s_env_len;
    }
    for (size_t i = 0; data == NULL && i < suit_cpayloads_count; i++) {
        if (strcmp(t->url, suit_cpayloads[i].uri) == 0) {
            data = compliance_start + suit_cpayloads[i].offset;
            len = suit_cpayloads[i].len;
        }
    }
    if (data == NULL) {
        return -ESP_ERR_NOT_FOUND;
    }
    size_t pos = t->offset + s->delivered;
    if (pos >= len) {
        return 0;
    }
    size_t n = len - pos < cap ? len - pos : cap;
    memcpy(buf, data + pos, n);
    s->delivered += n;
    return (int)n;
}

static void test_close(void *state)
{
    ((test_get_state_t *)state)->open = false;
}

/* ---------- campaigns ---------- */

/* hand over a whole resource the way a stream application does: every byte, then its end */
static esp_err_t feed_resource(esp_suit_ota_handle_t h, const uint8_t *data, size_t len, size_t chunk)
{
    size_t pos = 0;
    esp_err_t err = ESP_OK;
    while (pos < len) {
        size_t n = len - pos < chunk ? len - pos : chunk;
        size_t used = 0;
        err = esp_suit_ota_feed(h, data + pos, n, &used);
        pos += used;
        if (err == ESP_SUIT_OTA_READY && pos < len) {
            continue;                   /* bytes remain after the envelope: the next feed must refuse them */
        }
        if (err != ESP_OK) {
            break;
        }
        TEST_ASSERT_MESSAGE(used > 0, "feed used nothing and reported no status");
    }
    if (err != ESP_OK && err != ESP_SUIT_OTA_READY) {
        return err;
    }
    size_t used;
    return esp_suit_ota_feed(h, NULL, 0, &used);     /* the resource ended */
}

static esp_err_t pull(esp_suit_ota_handle_t h, esp_suit_ota_transport_t *t, size_t chunk)
{
    static uint8_t buf[512];
    if (chunk > sizeof(buf)) {
        chunk = sizeof(buf);
    }
    esp_err_t err = ESP_OK;
    for (int guard = 0; guard < 100000; guard++) {
        int n = esp_suit_ota_transport_get(t, buf, chunk);
        if (n < 0) {
            return (esp_err_t)(-n);
        }
        size_t off = 0;
        do {                            /* feed stops at a boundary that redirects t */
            size_t used = 0;
            err = esp_suit_ota_feed(h, buf + off, (size_t)n - off, &used);
            off += used;
        } while (err == ESP_OK && off < (size_t)n);
        if (err != ESP_OK || (n == 0 && t->done)) {
            return err;
        }
    }
    TEST_FAIL_MESSAGE("transport loop did not end");
    return ESP_FAIL;
}

esp_err_t run_campaign_keep(const esp_suit_ota_config_t *cfg, const uint8_t *env, size_t len, size_t chunk,
                            bool transport, esp_suit_ota_handle_t *out)
{
    esp_suit_ota_transport_t t = {
        .https = { .get = test_get, .close = test_close },
        .url = TEST_MANIFEST_URL,
    };
    s_env = env;
    s_env_len = len;
    esp_suit_ota_handle_t h;
    esp_err_t err = esp_suit_ota_begin(cfg, transport ? &t : NULL, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = transport ? pull(h, &t, chunk) : feed_resource(h, env, len, chunk);
    if (err == ESP_SUIT_OTA_READY) {
        esp_err_t c = esp_suit_ota_commit(h);
        if (c != ESP_OK) {
            err = c;
        }
    }
    if (err == ESP_SUIT_OTA_READY && out != NULL) {
        *out = h;
        return err;
    }
    esp_suit_ota_abort(h);
    return err;
}

esp_err_t run_campaign(const esp_suit_ota_config_t *cfg, const uint8_t *env, size_t len, size_t chunk, bool transport)
{
    return run_campaign_keep(cfg, env, len, chunk, transport, NULL);
}

esp_err_t run_cvec(const suit_cvec_t *v, size_t chunk)
{
    return run_campaign(&g_cfg, cvec_data(v), v->len, chunk, (v->flags & CVEC_TRANSPORT) != 0);
}
