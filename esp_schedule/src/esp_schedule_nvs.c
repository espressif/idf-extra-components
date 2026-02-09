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
static esp_schedule_priv_data_callbacks_t nvs_priv_data_callbacks = {
    .on_save = NULL,
    .on_load = NULL,
};

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
            ESP_SCHEDULE_LOGE(TAG, "NVS get existing schedule failed while adding schedule %s with error %d", schedule->name, err);
            esp_schedule_nvs_close(nvs_handle);
            return to_esp_schedule_return_type(err);
        }
    } else {
        ESP_SCHEDULE_LOGI(TAG, "Updating the existing schedule %s", schedule->name);
    }

    /* Calculate total blob size: persistent schedule + trigger list + private data */
    size_t total_size = sizeof(esp_schedule_persistent_t);
    size_t trigger_list_size = 0;
    size_t private_data_size = 0;
    if (schedule->triggers.count > 0 && schedule->triggers.list != NULL) {
        trigger_list_size = schedule->triggers.count * sizeof(esp_schedule_trigger_t);
        total_size += trigger_list_size;
    }

    /* Add private data size to total size if saving private data */
    if (nvs_priv_data_callbacks.on_save != NULL) {
        nvs_priv_data_callbacks.on_save(schedule->priv_data, NULL, &private_data_size);
        total_size += private_data_size;
    }

    /* Allocate buffer for combined data */
    uint8_t *blob_buffer = (uint8_t *)ESP_SCHEDULE_MALLOC(total_size);
    if (blob_buffer == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Could not allocate blob buffer");
        esp_schedule_nvs_close(nvs_handle);
        return ESP_SCHEDULE_RET_NO_MEM;
    }

    /* Create persistent schedule structure and copy to beginning of buffer */
    esp_schedule_persistent_t *persistent = (esp_schedule_persistent_t *) (blob_buffer);
    persistent->next_scheduled_time_diff = schedule->next_scheduled_time_diff;
    persistent->next_scheduled_time_utc = schedule->next_scheduled_time_utc;
    persistent->validity = schedule->validity;
    strlcpy(persistent->name, schedule->name, sizeof(persistent->name));
    persistent->triggers = schedule->triggers; // Copy trigger list metadata

    /* Copy trigger list to end of buffer if it exists */
    if (trigger_list_size > 0) {
        memcpy(blob_buffer + sizeof(esp_schedule_persistent_t), schedule->triggers.list, trigger_list_size);
    }

    /* Add private data to end of buffer if saving private data */
    if (nvs_priv_data_callbacks.on_save != NULL) {
        void *data = NULL;
        nvs_priv_data_callbacks.on_save(schedule->priv_data, &data, &private_data_size);
        if (data != NULL) {
            memcpy(blob_buffer + total_size - private_data_size, data, private_data_size);
            ESP_SCHEDULE_FREE(data);
        }
    }

    /* Store combined blob */
    err = esp_schedule_nvs_set_blob(nvs_handle, schedule->name, blob_buffer, total_size);
    ESP_SCHEDULE_FREE(blob_buffer);

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
                ESP_SCHEDULE_LOGE(TAG, "NVS get existing schedule count failed while adding schedule %s with error %d", schedule->name, err);
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

    /* Remove schedule blob (includes trigger data) */
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

static esp_schedule_handle_t esp_schedule_nvs_get(const char *nvs_key)
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

    /* Get blob size */
    err = esp_schedule_nvs_get_blob(nvs_handle, nvs_key, NULL, &buf_size);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS get failed with error %d", err);
        esp_schedule_nvs_close(nvs_handle);
        return NULL;
    }

    /* Allocate buffer for entire blob */
    uint8_t *blob_buffer = (uint8_t *)ESP_SCHEDULE_MALLOC(buf_size);
    if (blob_buffer == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Could not allocate blob buffer");
        esp_schedule_nvs_close(nvs_handle);
        return NULL;
    }

    /* Read entire blob */
    err = esp_schedule_nvs_get_blob(nvs_handle, nvs_key, blob_buffer, &buf_size);
    if (err != ESP_SCHEDULE_NVS_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS get failed with error %d", err);
        esp_schedule_nvs_close(nvs_handle);
        ESP_SCHEDULE_FREE(blob_buffer);
        return NULL;
    }

    /* Allocate schedule structure */
    esp_schedule_t *schedule = (esp_schedule_t *)ESP_SCHEDULE_CALLOC(1, sizeof(esp_schedule_t));
    if (schedule == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Could not allocate schedule");
        esp_schedule_nvs_close(nvs_handle);
        ESP_SCHEDULE_FREE(blob_buffer);
        return NULL;
    }

    /* Copy persistent schedule structure from beginning of blob */
    esp_schedule_persistent_t *persistent = (esp_schedule_persistent_t *) (blob_buffer);

    /* Reconstruct full schedule from persistent data */
    strlcpy(schedule->name, persistent->name, sizeof(schedule->name));
    schedule->next_scheduled_time_diff = persistent->next_scheduled_time_diff;
    schedule->next_scheduled_time_utc = persistent->next_scheduled_time_utc;
    schedule->validity = persistent->validity;
    schedule->triggers = persistent->triggers; // Copy trigger list metadata
    /* Runtime fields are zeroed by calloc: timer, trigger_cb, timestamp_cb, priv_data */

    /* Check if there are triggers to load */
    size_t schedule_size = sizeof(esp_schedule_persistent_t);
    size_t trigger_list_size = schedule->triggers.count * sizeof(esp_schedule_trigger_t);

    if (trigger_list_size > 0) {
        /* Allocate and load trigger list */
        esp_schedule_trigger_t *triggers = (esp_schedule_trigger_t *)ESP_SCHEDULE_MALLOC(trigger_list_size);
        if (triggers == NULL) {
            ESP_SCHEDULE_LOGE(TAG, "Could not allocate trigger list");
            esp_schedule_nvs_close(nvs_handle);
            ESP_SCHEDULE_FREE(schedule);
            ESP_SCHEDULE_FREE(blob_buffer);
            return NULL;
        }

        /* Copy trigger list from end of blob */
        memcpy(triggers, blob_buffer + schedule_size, trigger_list_size);

        /* Update schedule with loaded triggers */
        schedule->triggers.list = triggers;
        ESP_SCHEDULE_LOGI(TAG, "Loaded %d triggers for schedule %s", schedule->triggers.count, schedule->name);
    } else {
        /* No trigger list - set to NULL and count to 0 */
        uint8_t original_count = schedule->triggers.count;
        schedule->triggers.list = 0x0;
        schedule->triggers.count = 0;
        if (original_count > 0) {
            ESP_SCHEDULE_LOGW(TAG, "Schedule %s has trigger count but no trigger data in blob", nvs_key);
        }
    }

    /* Load private data if saving private data */
    size_t data_len = buf_size - schedule_size - trigger_list_size;
    if (data_len > 0 && nvs_priv_data_callbacks.on_load != NULL) {
        void *data = (void *) blob_buffer + schedule_size + trigger_list_size;
        nvs_priv_data_callbacks.on_load(data, data_len, &schedule->priv_data);
    } else {
        /* No private data to load */
        schedule->priv_data = NULL;
    }

    esp_schedule_nvs_close(nvs_handle);
    ESP_SCHEDULE_FREE(blob_buffer);
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
    esp_schedule_handle_t *handle_list = (esp_schedule_handle_t *)ESP_SCHEDULE_CALLOC(*schedule_count, sizeof(esp_schedule_handle_t));
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
        *schedule_count = 0;
        return NULL;
    }
    while (err == ESP_SCHEDULE_NVS_OK) {
        char *next_key = NULL;
        esp_schedule_nvs_entry_get_key(nvs_iterator, &next_key);
        if (next_key == NULL) {
            break;
        }
        ESP_SCHEDULE_LOGI(TAG, "Found schedule in NVS with key: %s", next_key);
        handle_list[handle_count] = esp_schedule_nvs_get(next_key);
        if (handle_list[handle_count] != NULL) {
            /* Increase count only if nvs_get was successful */
            handle_count++;
        }
        ESP_SCHEDULE_FREE(next_key);
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

ESP_SCHEDULE_RETURN_TYPE esp_schedule_nvs_init(char *nvs_partition, esp_schedule_priv_data_callbacks_t *priv_data_callbacks)
{
    if (nvs_enabled) {
        ESP_SCHEDULE_LOGI(TAG, "NVS already enabled");
        return ESP_SCHEDULE_RET_OK;
    }
    if (nvs_partition) {
        esp_schedule_nvs_partition = strndup(nvs_partition, strlen(nvs_partition));
    } else {
        esp_schedule_nvs_partition = strndup("nvs", strlen("nvs"));
    }
    if (esp_schedule_nvs_partition == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Could not allocate nvs_partition");
        return ESP_SCHEDULE_RET_NO_MEM;
    }
    if (priv_data_callbacks != NULL) {
        nvs_priv_data_callbacks = *priv_data_callbacks;
    }
    nvs_enabled = true;
    return ESP_SCHEDULE_RET_OK;
}
