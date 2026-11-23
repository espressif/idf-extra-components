/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The adapter that lets esp_suit_ota use the declared pairs of this component.
 *
 * esp_suit_ota knows nothing about pairs, records or groups. It asks a backend for the
 * partitions of a named target, and at the end of activate() it asks the backend to record the
 * new indices. This file is that backend. It compiles only when esp_suit_ota is in the build.
 */

#include <string.h>
#include "esp_log.h"
#include "esp_suit_ota_slots.h"
#include "esp_ota_slots.h"
#include "esp_partition.h"

static const char *TAG = "esp_suit_ota_slots";

#define MAX_FLIPS CONFIG_ESP_SUIT_OTA_MAX_COMPONENTS

/*
 * esp_ota_slots_init() fails when its record cannot be trusted, and leaves the component
 * uninitialized. Every call then answers ESP_ERR_OTA_SLOTS_STATE. Turn that into
 * ESP_ERR_INVALID_STATE, which is what the core reads as "this backend is not usable".
 */
static bool slots_ready(void)
{
    esp_ota_slots_state_t st;
    if (esp_ota_slots_get_state(&st, NULL, NULL) == ESP_OK) {
        return true;
    }
    ESP_LOGE(TAG, "esp_ota_slots is not initialized: call esp_ota_slots_init() and check its result");
    return false;
}

static esp_err_t slots_resolve(void *user, const char *name, esp_suit_ota_target_t *out)
{
    (void)user;
    if (!slots_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_ota_slots_kind_t kind = esp_ota_slots_get_kind(name);
    if (kind == ESP_OTA_SLOTS_NONE) {
        return ESP_ERR_NOT_FOUND;
    }
    out->staging = esp_ota_slots_get_staging(name);
    out->active = esp_ota_slots_get_active(name);
    if (out->staging == NULL || out->active == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    out->is_app = (kind == ESP_OTA_SLOTS_APP);
    out->copy_to_active = (kind == ESP_OTA_SLOTS_COPY);
    out->active_index = esp_ota_slots_get_active_index(name);
    if (out->is_app) {
        out->slot_index = (uint8_t)(out->staging->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
    } else if (kind == ESP_OTA_SLOTS_AB) {
        out->slot_index = (uint8_t)(1 - out->active_index);
    } else {
        out->slot_index = 0;
    }
    return ESP_OK;
}

static esp_err_t slots_check_set(void *user, const char *const *names, size_t n)
{
    (void)user;
    if (!slots_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_ota_slots_check_group(names, n);
}

static esp_err_t slots_commit(void *user, const esp_suit_ota_flip_t *flips, size_t n,
                              uint8_t app_slot, uint32_t campaign_id)
{
    (void)user;
    if (!slots_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (n > MAX_FLIPS) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *names[MAX_FLIPS];
    uint8_t idx[MAX_FLIPS];
    for (size_t i = 0; i < n; i++) {
        names[i] = flips[i].name;
        idx[i] = flips[i].new_index;
    }
    return esp_ota_slots_commit_flip(names, idx, n, app_slot, campaign_id);
}

static esp_err_t slots_activated(void *user)
{
    (void)user;
    return esp_ota_slots_activation_done();
}

static esp_err_t slots_revert(void *user)
{
    (void)user;
    return esp_ota_slots_revert();
}

static bool slots_is_pending(void *user)
{
    (void)user;
    esp_ota_slots_state_t st;
    return esp_ota_slots_get_state(&st, NULL, NULL) == ESP_OK && st != ESP_OTA_SLOTS_CONFIRMED;
}

static const esp_suit_ota_slots_ops_t s_ops = {
    .resolve = slots_resolve,
    .check_set = slots_check_set,
    .commit = slots_commit,
    .activated = slots_activated,
    .revert = slots_revert,
    .is_pending = slots_is_pending,
    .user = NULL,
};

const esp_suit_ota_slots_ops_t *esp_suit_ota_slots_ops(void)
{
    return &s_ops;
}
