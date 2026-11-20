/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "suit_priv.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = SUIT_TAG;

#define NS            "esp_suit"
#define KEY_JOURNAL   "jrnl"
#define KEY_INSTALLED "inst"
#define KEY_REJECTED  "rej"
#define KEY_ETAG      "etag"
#define JOURNAL_MAGIC 0x4A535355u   /* 'USSJ' */
#define INSTALLED_MAGIC 0x49535355u /* 'USSI' */
#define REJECTED_RING 8

typedef struct {
    uint8_t next;
    uint8_t digest[REJECTED_RING][SUIT_DIGEST_LEN];
} rejected_t;

static esp_err_t read_blob(const char *key, void *out, size_t len)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    size_t got = len;
    err = nvs_get_blob(h, key, out, &got);
    nvs_close(h);
    if (err != ESP_OK || got != len) {
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

static esp_err_t write_blob(const char *key, const void *data, size_t len)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_blob(h, key, data, len);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

static esp_err_t erase_key(const char *key)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_key(h, key);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    nvs_close(h);
    return err;
}

esp_err_t suit_persist_journal_read(suit_journal_t *j)
{
    esp_err_t err = read_blob(KEY_JOURNAL, j, sizeof(*j));
    if (err != ESP_OK || j->magic != JOURNAL_MAGIC) {
        memset(j, 0, sizeof(*j));
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

esp_err_t suit_persist_journal_write(const suit_journal_t *j)
{
    return write_blob(KEY_JOURNAL, j, sizeof(*j));
}

esp_err_t suit_persist_journal_erase(void)
{
    return erase_key(KEY_JOURNAL);
}

esp_err_t suit_persist_installed_read(suit_installed_t *r)
{
    esp_err_t err = read_blob(KEY_INSTALLED, r, sizeof(*r));
    if (err != ESP_OK || r->magic != INSTALLED_MAGIC) {
        memset(r, 0, sizeof(*r));
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

esp_err_t suit_persist_installed_write(const suit_installed_t *r)
{
    suit_installed_t copy = *r;
    copy.magic = INSTALLED_MAGIC;
    return write_blob(KEY_INSTALLED, &copy, sizeof(copy));
}

uint64_t suit_persist_installed_sequence(void)
{
    suit_installed_t r;
    uint64_t floor = CONFIG_ESP_SUIT_OTA_INITIAL_SEQ;
    if (suit_persist_installed_read(&r) == ESP_OK && r.sequence_number > floor) {
        return r.sequence_number;
    }
    return floor;
}

bool suit_persist_is_rejected(const uint8_t digest[SUIT_DIGEST_LEN])
{
    rejected_t r;
    if (read_blob(KEY_REJECTED, &r, sizeof(r)) != ESP_OK) {
        return false;
    }
    for (int i = 0; i < REJECTED_RING; i++) {
        if (memcmp(r.digest[i], digest, SUIT_DIGEST_LEN) == 0) {
            return true;
        }
    }
    return false;
}

esp_err_t suit_persist_add_rejected(const uint8_t digest[SUIT_DIGEST_LEN])
{
    rejected_t r;
    if (read_blob(KEY_REJECTED, &r, sizeof(r)) != ESP_OK) {
        memset(&r, 0, sizeof(r));
    }
    if (suit_persist_is_rejected(digest)) {
        return ESP_OK;
    }
    memcpy(r.digest[r.next % REJECTED_RING], digest, SUIT_DIGEST_LEN);
    r.next = (uint8_t)((r.next + 1) % REJECTED_RING);
    return write_blob(KEY_REJECTED, &r, sizeof(r));
}

esp_err_t suit_persist_etag_read(char *out, size_t cap)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    size_t len = cap;
    esp_err_t err = nvs_get_str(h, KEY_ETAG, out, &len);
    nvs_close(h);
    return err == ESP_OK ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t suit_persist_etag_write(const char *etag)
{
    if (etag == NULL || etag[0] == '\0') {
        return erase_key(KEY_ETAG);
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(h, KEY_ETAG, etag);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

/* the journal as the core sees it: built from the live campaign, then written */
esp_err_t suit_journal_save(struct esp_suit_ota_ctx *ctx, suit_phase_t phase)
{
    suit_journal_t *j = &ctx->journal;
    memset(j, 0, sizeof(*j));
    j->magic = 0x4A535355u;
    j->phase = (uint8_t)phase;
    j->num = (uint8_t)ctx->num_comp;
    j->current = ctx->current == SIZE_MAX ? SUIT_COMPONENT_NONE : (uint8_t)ctx->current;
    j->app_slot = ctx->has_app ? ctx->app_slot : ESP_OTA_SLOTS_APP_SLOT_NONE;
    j->campaign_id = ctx->campaign_id;
    j->bytes_written = ctx->current == SIZE_MAX ? 0 : ctx->comp[ctx->current].bytes_written;
    j->sequence_number = ctx->man.sequence_number;
    memcpy(j->manifest_digest, ctx->manifest_digest, SUIT_DIGEST_LEN);
    for (size_t i = 0; i < ctx->num_comp; i++) {
        suit_component_t *c = &ctx->comp[i];
        suit_journal_comp_t *jc = &j->comp[i];
        strlcpy(jc->slots, c->target.slots, sizeof(jc->slots));
        jc->kind = c->target.slots[0] ? (uint8_t)esp_ota_slots_get_kind(c->target.slots) : 0;
        jc->new_index = c->target.slot_index;
        jc->state = (uint8_t)c->state;
        jc->is_app = c->target.is_app;
        jc->size = c->size;
        memcpy(jc->digest, c->digest, SUIT_DIGEST_LEN);
    }
    esp_err_t err = suit_persist_journal_write(j);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "journal not saved: %s", esp_err_to_name(err));
    }
    return err;
}
