/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file nvs.c
 * @brief nvs_flash implementation of esp_schedule_nvs_ops_t.
 *
 * @note Reached only through esp_schedule_esp_nvs_ops, which only
 *       esp_schedule_init() references. A build that installs its own port
 *       never links this file, and with it never pulls in nvs_flash.
 */

#include "esp_schedule.h"
#include "esp_schedule_esp_port.h"

#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "esp_schedule_nvs_port";

/* nvs_handle_t is an integer, not a pointer, so round-trip it through uintptr_t
 * rather than casting an integer straight to void *. */
static nvs_handle_t to_nvs_handle(esp_schedule_store_handle_t handle)
{
    return (nvs_handle_t)(uintptr_t)handle;
}

static esp_err_t esp_nvs_open(const char *partition, const char *name_space, bool readonly,
                              esp_schedule_store_handle_t *p_handle)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(partition, name_space,
                                            readonly ? NVS_READONLY : NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    *p_handle = (esp_schedule_store_handle_t)(uintptr_t)handle;
    return ESP_OK;
}

static void esp_nvs_close(esp_schedule_store_handle_t handle)
{
    /* close() is the commit point in the port contract: esp_schedule batches
     * writes between open() and close() and expects them durable once close()
     * returns. A failed commit is only worth logging here - the handle has to
     * be released either way. */
    esp_err_t err = nvs_commit(to_nvs_handle(handle));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS commit failed with error %d", err);
    }
    nvs_close(to_nvs_handle(handle));
}

static esp_err_t esp_nvs_read(esp_schedule_store_handle_t handle, const char *key,
                              void *value, size_t *p_value_len)
{
    return nvs_get_blob(to_nvs_handle(handle), key, value, p_value_len);
}

static esp_err_t esp_nvs_write(esp_schedule_store_handle_t handle, const char *key,
                               const void *value, size_t value_len)
{
    return nvs_set_blob(to_nvs_handle(handle), key, value, value_len);
}

static esp_err_t esp_nvs_erase(esp_schedule_store_handle_t handle, const char *key)
{
    if (key == NULL) {
        return nvs_erase_all(to_nvs_handle(handle));
    }
    return nvs_erase_key(to_nvs_handle(handle), key);
}

static esp_err_t esp_nvs_foreach_key(const char *partition, const char *name_space,
                                     esp_schedule_key_cb_t cb, void *ctx)
{
    nvs_iterator_t it = NULL;
    esp_err_t err = nvs_entry_find(partition, name_space, NVS_TYPE_BLOB, &it);
    if (err != ESP_OK) {
        /* An empty namespace is not an error to the caller, just an empty walk. */
        return (err == ESP_ERR_NVS_NOT_FOUND) ? ESP_OK : err;
    }
    while (err == ESP_OK && it != NULL) {
        nvs_entry_info_t info;
        err = nvs_entry_info(it, &info);
        if (err != ESP_OK) {
            break;
        }
        /* info.key is a fixed array inside info, so it stays valid for the
         * duration of the callback - exactly the lifetime the port contract
         * promises, and it keeps iteration allocation-free. */
        if (!cb(info.key, ctx)) {
            break;
        }
        err = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
    /* Running off the end of the namespace is the normal way to finish. */
    return (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) ? ESP_OK : err;
}

/* Operations table ************************************************************
 *
 * The only exported symbol in this file. esp_schedule_init() is its sole
 * referrer, so nothing here is linked into a build that installs its own port. */
const esp_schedule_nvs_ops_t esp_schedule_esp_nvs_ops = {
    .open = esp_nvs_open,
    .close = esp_nvs_close,
    .read = esp_nvs_read,
    .write = esp_nvs_write,
    .erase = esp_nvs_erase,
    .foreach_key = esp_nvs_foreach_key,
};
