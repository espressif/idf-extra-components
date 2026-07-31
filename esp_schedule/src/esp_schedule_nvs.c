// Copyright 2025 Espressif Systems (Shanghai) CO LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string.h>
#include <time.h>
#include "esp_schedule_internal.h"
#include "glue_mem.h"
#include "glue_nvs.h"
#include "glue_log.h"

static const char *TAG = "esp_schedule_nvs";

#define ESP_SCHEDULE_NVS_NAMESPACE "schd"
#define ESP_SCHEDULE_COUNT_KEY "schd_count"

static char *esp_schedule_nvs_partition = NULL;
static bool nvs_enabled = false;

static ESP_SCHEDULE_RETURN_TYPE to_esp_schedule_return_type(esp_schedule_nvs_error_t err)
{
    switch (err) {
    case ESP_SCHEDULE_NVS_OK:
        return ESP_SCHEDULE_RET_OK;
    case ESP_SCHEDULE_NVS_NOT_FOUND:
        return ESP_SCHEDULE_RET_INVALID_STATE;
    case ESP_SCHEDULE_NVS_NO_MEM:
        return ESP_SCHEDULE_RET_NO_MEM;
    default:
        return ESP_SCHEDULE_RET_FAIL;
    }
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_nvs_add(esp_schedule_t *schedule)
{
    if (!nvs_enabled) {
        ESP_SCHEDULE_LOGD(TAG, "NVS not enabled. Not adding to NVS.");
        return ESP_SCHEDULE_RET_INVALID_STATE;
    }
    esp_schedule_nvs_handle_t nvs_handle;
    esp_schedule_nvs_error_t err = esp_schedule_nvs_open_from_partition(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, ESP_SCHEDULE_NVS_OPEN_READWRITE, &nvs_handle);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS open failed with error %d", err);
        return to_esp_schedule_return_type(err);
    }

    /* Check if this is new schedule or editing an existing schedule */
    size_t buf_size;
    bool editing_schedule = true;
    err = esp_schedule_nvs_get_blob(nvs_handle, schedule->name, NULL, &buf_size);
    if (err != ESP_SCHEDULE_NVS_OK) {
        if (err == ESP_SCHEDULE_NVS_NOT_FOUND) {
            editing_schedule = false;
        } else {
            ESP_SCHEDULE_LOGE(TAG, "NVS get failed with error %d", err);
            esp_schedule_nvs_close(nvs_handle);
            return to_esp_schedule_return_type(err);
        }
    } else {
        ESP_SCHEDULE_LOGI(TAG, "Updating the existing schedule %s", schedule->name);
    }

    err = esp_schedule_nvs_set_blob(nvs_handle, schedule->name, schedule, sizeof(esp_schedule_t));
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS set failed with error %d", err);
        esp_schedule_nvs_close(nvs_handle);
        return to_esp_schedule_return_type(err);
    }
    if (editing_schedule == false) {
        uint8_t schedule_count;
        err = esp_schedule_nvs_get_u8(nvs_handle, ESP_SCHEDULE_COUNT_KEY, &schedule_count);
        if (err != ESP_SCHEDULE_NVS_OK) {
            if (err == ESP_SCHEDULE_NVS_NOT_FOUND) {
                schedule_count = 0;
            } else {
                ESP_SCHEDULE_LOGE(TAG, "NVS get failed with error %d", err);
                esp_schedule_nvs_close(nvs_handle);
                return to_esp_schedule_return_type(err);
            }
        }
        schedule_count++;
        err = esp_schedule_nvs_set_u8(nvs_handle, ESP_SCHEDULE_COUNT_KEY, schedule_count);
        if (err != ESP_SCHEDULE_NVS_OK) {
            ESP_SCHEDULE_LOGE(TAG, "NVS set failed for schedule count with error %d", err);
            esp_schedule_nvs_close(nvs_handle);
            return to_esp_schedule_return_type(err);
        }
    }
    esp_schedule_nvs_commit(nvs_handle);
    esp_schedule_nvs_close(nvs_handle);
    ESP_SCHEDULE_LOGI(TAG, "Schedule %s added in NVS", schedule->name);
    return ESP_SCHEDULE_RET_OK;
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_nvs_remove_all(void)
{
    if (!nvs_enabled) {
        ESP_SCHEDULE_LOGD(TAG, "NVS not enabled. Not removing from NVS.");
        return ESP_SCHEDULE_RET_INVALID_STATE;
    }
    esp_schedule_nvs_handle_t nvs_handle;
    esp_schedule_nvs_error_t err = esp_schedule_nvs_open_from_partition(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, ESP_SCHEDULE_NVS_OPEN_READWRITE, &nvs_handle);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS open failed with error %d", err);
        return to_esp_schedule_return_type(err);
    }
    err = esp_schedule_nvs_erase_all(nvs_handle);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS erase all keys failed with error %d", err);
        esp_schedule_nvs_close(nvs_handle);
        return to_esp_schedule_return_type(err);
    }
    esp_schedule_nvs_commit(nvs_handle);
    esp_schedule_nvs_close(nvs_handle);
    ESP_SCHEDULE_LOGI(TAG, "All schedules removed from NVS");
    return ESP_SCHEDULE_RET_OK;
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_nvs_remove(esp_schedule_t *schedule)
{
    if (!nvs_enabled) {
        ESP_SCHEDULE_LOGD(TAG, "NVS not enabled. Not removing from NVS.");
        return ESP_SCHEDULE_RET_INVALID_STATE;
    }
    esp_schedule_nvs_handle_t nvs_handle;
    esp_schedule_nvs_error_t err = esp_schedule_nvs_open_from_partition(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, ESP_SCHEDULE_NVS_OPEN_READWRITE, &nvs_handle);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS open failed with error %d", err);
        return to_esp_schedule_return_type(err);
    }
    err = esp_schedule_nvs_erase_key(nvs_handle, schedule->name);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS erase key failed with error %d", err);
        esp_schedule_nvs_close(nvs_handle);
        return to_esp_schedule_return_type(err);
    }
    uint8_t schedule_count;
    err = esp_schedule_nvs_get_u8(nvs_handle, ESP_SCHEDULE_COUNT_KEY, &schedule_count);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS get failed for schedule count with error %d", err);
        esp_schedule_nvs_close(nvs_handle);
        return to_esp_schedule_return_type(err);
    }
    schedule_count--;
    err = esp_schedule_nvs_set_u8(nvs_handle, ESP_SCHEDULE_COUNT_KEY, schedule_count);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS set failed for schedule count with error %d", err);
        esp_schedule_nvs_close(nvs_handle);
        return to_esp_schedule_return_type(err);
    }
    esp_schedule_nvs_commit(nvs_handle);
    esp_schedule_nvs_close(nvs_handle);
    ESP_SCHEDULE_LOGI(TAG, "Schedule %s removed from NVS", schedule->name);
    return ESP_SCHEDULE_RET_OK;
}

static uint8_t esp_schedule_nvs_get_count(void)
{
    if (!nvs_enabled) {
        ESP_SCHEDULE_LOGD(TAG, "NVS not enabled. Not getting count from NVS.");
        return 0;
    }
    esp_schedule_nvs_handle_t nvs_handle;
    esp_schedule_nvs_error_t err = esp_schedule_nvs_open_from_partition(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, ESP_SCHEDULE_NVS_OPEN_READONLY, &nvs_handle);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS open failed with error %d", err);
        return 0;
    }
    uint8_t schedule_count;
    err = esp_schedule_nvs_get_u8(nvs_handle, ESP_SCHEDULE_COUNT_KEY, &schedule_count);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS get failed for schedule count with error %d", err);
        esp_schedule_nvs_close(nvs_handle);
        return 0;
    }
    esp_schedule_nvs_close(nvs_handle);
    ESP_SCHEDULE_LOGI(TAG, "Schedules in NVS: %d", schedule_count);
    return schedule_count;
}

static esp_schedule_handle_t esp_schedule_nvs_get(char *nvs_key)
{
    if (!nvs_enabled) {
        ESP_SCHEDULE_LOGD(TAG, "NVS not enabled. Not getting from NVS.");
        return NULL;
    }
    size_t buf_size;
    esp_schedule_nvs_handle_t nvs_handle;
    esp_schedule_nvs_error_t err = esp_schedule_nvs_open_from_partition(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, ESP_SCHEDULE_NVS_OPEN_READONLY, &nvs_handle);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS open failed with error %d", err);
        return NULL;
    }
    err = esp_schedule_nvs_get_blob(nvs_handle, nvs_key, NULL, &buf_size);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS get failed with error %d", err);
        esp_schedule_nvs_close(nvs_handle);
        return NULL;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)ESP_SCHEDULE_MALLOC(buf_size);
    if (schedule == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Could not allocate handle");
        esp_schedule_nvs_close(nvs_handle);
        return NULL;
    }
    err = esp_schedule_nvs_get_blob(nvs_handle, nvs_key, schedule, &buf_size);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS get failed with error %d", err);
        esp_schedule_nvs_close(nvs_handle);
        ESP_SCHEDULE_FREE(schedule);
        return NULL;
    }
    esp_schedule_nvs_close(nvs_handle);
    ESP_SCHEDULE_LOGI(TAG, "Schedule %s found in NVS", schedule->name);
    return (esp_schedule_handle_t) schedule;
}

esp_schedule_handle_t *esp_schedule_nvs_get_all(uint8_t *schedule_count)
{
    if (!nvs_enabled) {
        ESP_SCHEDULE_LOGD(TAG, "NVS not enabled. Not Initialising NVS.");
        return NULL;
    }

    *schedule_count = esp_schedule_nvs_get_count();
    if (*schedule_count == 0) {
        ESP_SCHEDULE_LOGI(TAG, "No Entries found in NVS");
        return NULL;
    }
    esp_schedule_handle_t *handle_list = (esp_schedule_handle_t *)ESP_SCHEDULE_MALLOC(sizeof(esp_schedule_handle_t) * (*schedule_count));
    if (handle_list == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Could not allocate schedule list");
        *schedule_count = 0;
        return NULL;
    }
    int handle_count = 0;

    esp_schedule_nvs_iterator_t nvs_iterator = NULL;
    esp_schedule_nvs_error_t err = esp_schedule_nvs_entry_find_blobs(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, &nvs_iterator);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "No entry found in NVS");
        ESP_SCHEDULE_FREE(handle_list);
        return NULL;
    }
    while (err == ESP_SCHEDULE_NVS_OK) {
        char *nvs_key = NULL;
        esp_schedule_nvs_entry_get_key(nvs_iterator, &nvs_key);
        if (nvs_key == NULL) {
            break;
        }
        ESP_SCHEDULE_LOGI(TAG, "Found schedule in NVS with key: %s", nvs_key);
        handle_list[handle_count] = esp_schedule_nvs_get(nvs_key);
        if (handle_list[handle_count] != NULL) {
            /* Increase count only if nvs_get was successful */
            handle_count++;
        }
        ESP_SCHEDULE_FREE(nvs_key);
        err = esp_schedule_nvs_entry_next(&nvs_iterator);
    }
    esp_schedule_nvs_release_iterator(nvs_iterator);
    *schedule_count = handle_count;
    ESP_SCHEDULE_LOGI(TAG, "Found %d schedules in NVS", *schedule_count);
    return handle_list;
}

bool esp_schedule_nvs_is_enabled(void)
{
    return nvs_enabled;
}

ESP_SCHEDULE_RETURN_TYPE esp_schedule_nvs_init(char *nvs_partition)
{
    if (nvs_enabled) {
        ESP_SCHEDULE_LOGI(TAG, "NVS already enabled");
        return ESP_SCHEDULE_RET_OK;
    }
    /* Copied through the glue allocator rather than strdup() so a port that
     * overrides ESP_SCHEDULE_MALLOC keeps every allocation on one heap. */
    const char *partition = nvs_partition ? nvs_partition : "nvs";
    size_t partition_len = strlen(partition);
    esp_schedule_nvs_partition = (char *)ESP_SCHEDULE_MALLOC(partition_len + 1);
    if (esp_schedule_nvs_partition != NULL) {
        memcpy(esp_schedule_nvs_partition, partition, partition_len + 1);
    }
    if (esp_schedule_nvs_partition == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Could not allocate nvs_partition");
        return ESP_SCHEDULE_RET_NO_MEM;
    }
    nvs_enabled = true;
    return ESP_SCHEDULE_RET_OK;
}
