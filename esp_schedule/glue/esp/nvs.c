/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_nvs.c
 * @brief ESP NVS implementation.
 */

#include "nvs_flash.h"
#include "glue_nvs.h"

#include <string.h>

static nvs_open_mode_t to_nvs_open_mode(esp_schedule_nvs_open_mode_t mode)
{
    switch (mode) {
    case ESP_SCHEDULE_NVS_OPEN_READONLY:
        return NVS_READONLY;
    case ESP_SCHEDULE_NVS_OPEN_READWRITE:
        return NVS_READWRITE;
    default:
        return NVS_READWRITE;
    }
}

static esp_schedule_nvs_error_t to_esp_schedule_nvs_error(esp_err_t err)
{
    switch (err) {
    case ESP_OK:
        return ESP_SCHEDULE_NVS_OK;
    case ESP_ERR_NVS_NOT_FOUND:
        return ESP_SCHEDULE_NVS_NOT_FOUND;
    default:
        return ESP_SCHEDULE_NVS_ERROR;
    }
}

esp_schedule_nvs_error_t esp_schedule_nvs_open_from_partition(const char *partition_label, const char *name_space, esp_schedule_nvs_open_mode_t mode, esp_schedule_nvs_handle_t *p_handle)
{
    return to_esp_schedule_nvs_error(nvs_open_from_partition(partition_label, name_space, to_nvs_open_mode(mode), (nvs_handle_t *) p_handle));
}

void esp_schedule_nvs_close(esp_schedule_nvs_handle_t handle)
{
    nvs_close((nvs_handle_t) handle);
}

esp_schedule_nvs_error_t esp_schedule_nvs_commit(esp_schedule_nvs_handle_t handle)
{
    return to_esp_schedule_nvs_error(nvs_commit((nvs_handle_t) handle));
}

esp_schedule_nvs_error_t esp_schedule_nvs_erase_key(esp_schedule_nvs_handle_t handle, const char *key)
{
    return to_esp_schedule_nvs_error(nvs_erase_key((nvs_handle_t) handle, key));
}

esp_schedule_nvs_error_t esp_schedule_nvs_erase_all(esp_schedule_nvs_handle_t handle)
{
    return to_esp_schedule_nvs_error(nvs_erase_all((nvs_handle_t) handle));
}

esp_schedule_nvs_error_t esp_schedule_nvs_set_blob(esp_schedule_nvs_handle_t handle, const char *key, const void *value, size_t value_len)
{
    return to_esp_schedule_nvs_error(nvs_set_blob((nvs_handle_t) handle, key, value, value_len));
}

esp_schedule_nvs_error_t esp_schedule_nvs_get_blob(esp_schedule_nvs_handle_t handle, const char *key, void *value, size_t *p_value_len)
{
    return to_esp_schedule_nvs_error(nvs_get_blob((nvs_handle_t) handle, key, value, p_value_len));
}

esp_schedule_nvs_error_t esp_schedule_nvs_set_u8(esp_schedule_nvs_handle_t handle, const char *key, uint8_t value)
{
    return to_esp_schedule_nvs_error(nvs_set_u8((nvs_handle_t) handle, key, value));
}

esp_schedule_nvs_error_t esp_schedule_nvs_get_u8(esp_schedule_nvs_handle_t handle, const char *key, uint8_t *value)
{
    return to_esp_schedule_nvs_error(nvs_get_u8((nvs_handle_t) handle, key, value));
}

esp_schedule_nvs_error_t esp_schedule_nvs_entry_find_blobs(const char *partition_label, const char *name_space, esp_schedule_nvs_iterator_t *iterator)
{
    return to_esp_schedule_nvs_error(nvs_entry_find(partition_label, name_space, NVS_TYPE_BLOB, (nvs_iterator_t *) iterator));
}

esp_schedule_nvs_error_t esp_schedule_nvs_entry_get_key(esp_schedule_nvs_iterator_t iterator, char **p_key)
{
    nvs_entry_info_t nvs_entry;
    esp_err_t err = nvs_entry_info((nvs_iterator_t) iterator, &nvs_entry);
    if (err != ESP_OK) {
        return to_esp_schedule_nvs_error(err);
    }
    size_t key_len = strlen(nvs_entry.key);
    *p_key = (char *)malloc(key_len + 1);
    if (*p_key == NULL) {
        return ESP_SCHEDULE_NVS_NO_MEM;
    }
    memcpy(*p_key, nvs_entry.key, key_len + 1);
    return ESP_SCHEDULE_NVS_OK;
}

esp_schedule_nvs_error_t esp_schedule_nvs_entry_next(esp_schedule_nvs_iterator_t *iterator)
{
    return to_esp_schedule_nvs_error(nvs_entry_next((nvs_iterator_t *) iterator));
}

void esp_schedule_nvs_release_iterator(esp_schedule_nvs_iterator_t iterator)
{
    nvs_release_iterator((nvs_iterator_t) iterator);
}