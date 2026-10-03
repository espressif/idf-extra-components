/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file blemesh_profile_store.c
 * @brief NVS store for node profiles that Composition Data alone cannot give
 *        (sensor kinds from Sensor Descriptors), keyed by primary address.
 */

#include <stdio.h>

#include "esp_log.h"
#include "nvs.h"

#include "blemesh_internal.h"

#define TAG "blemesh_prof"

#define PROFILE_NVS_NAMESPACE "blemesh_prof"

static void profile_key(blemesh_addr_t addr, char key[NVS_KEY_NAME_MAX_SIZE])
{
    snprintf(key, NVS_KEY_NAME_MAX_SIZE, "prof_%04x", addr);
}

esp_err_t blemesh_profile_store_set(blemesh_addr_t addr, blemesh_device_profile_t profile)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(PROFILE_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }
    char key[NVS_KEY_NAME_MAX_SIZE];
    profile_key(addr, key);
    err = nvs_set_u8(h, key, (uint8_t)profile);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "profile store for 0x%04x failed: %s", addr, esp_err_to_name(err));
    }
    return err;
}

esp_err_t blemesh_profile_store_get(blemesh_addr_t addr, blemesh_device_profile_t *profile)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(PROFILE_NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return err;
    }
    char key[NVS_KEY_NAME_MAX_SIZE];
    profile_key(addr, key);
    uint8_t v = 0;
    err = nvs_get_u8(h, key, &v);
    nvs_close(h);
    if (err == ESP_OK) {
        *profile = (blemesh_device_profile_t)v;
    }
    return err;
}

void blemesh_profile_store_erase(blemesh_addr_t addr)
{
    nvs_handle_t h;
    if (nvs_open(PROFILE_NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    char key[NVS_KEY_NAME_MAX_SIZE];
    profile_key(addr, key);
    if (nvs_erase_key(h, key) == ESP_OK) {
        (void)nvs_commit(h);
    }
    nvs_close(h);
}

esp_err_t blemesh_profile_store_erase_all(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(PROFILE_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_all(h);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}
