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
#define KEY_INSTALLED "inst"
#define KEY_ETAG      "etag"
#define INSTALLED_MAGIC 0x49535355u /* 'USSI' */

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
