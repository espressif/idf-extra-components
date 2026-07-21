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
#include "esp_log.h"
#include "nvs.h"
#include "esp_schedule_internal.h"

static const char *TAG = "esp_schedule_nvs";

#define ESP_SCHEDULE_NVS_NAMESPACE "schd"
#define ESP_SCHEDULE_COUNT_KEY "schd_count"

static char *esp_schedule_nvs_partition = NULL;
static bool nvs_enabled = false;
static esp_schedule_priv_data_callbacks_t nvs_priv_data_callbacks = {
    .on_save = NULL,
    .on_load = NULL,
    .on_free = NULL,
};

esp_err_t esp_schedule_nvs_add(esp_schedule_t *schedule)
{
    if (!nvs_enabled) {
        ESP_LOGD(TAG, "NVS not enabled. Not adding to NVS.");
        return ESP_ERR_INVALID_STATE;
    }
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open_from_partition(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed with error %d", err);
        return err;
    }

    /* Check if this is new schedule or editing an existing schedule */
    size_t buf_size;
    bool editing_schedule = true;
    err = nvs_get_blob(nvs_handle, schedule->name, NULL, &buf_size);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            editing_schedule = false;
        } else {
            ESP_LOGE(TAG, "NVS get existing schedule failed while adding schedule %s with error %d", schedule->name, err);
            nvs_close(nvs_handle);
            return err;
        }
    } else {
        ESP_LOGI(TAG, "Updating the existing schedule %s", schedule->name);
    }

    /* Calculate total blob size: the persisted struct + any private data */
    size_t total_size = sizeof(esp_schedule_persistent_t);
    size_t private_data_size = 0;

    /* Add private data size to total size if saving private data */
    if (nvs_priv_data_callbacks.on_save != NULL) {
        nvs_priv_data_callbacks.on_save(schedule->priv_data, NULL, &private_data_size);
        total_size += private_data_size;
    }

    /* Allocate buffer for combined data */
    uint8_t *blob_buffer = (uint8_t *)malloc(total_size);
    if (blob_buffer == NULL) {
        ESP_LOGE(TAG, "Could not allocate blob buffer");
        nvs_close(nvs_handle);
        return ESP_ERR_NO_MEM;
    }

    /* Build the persisted form at the start of the buffer. Runtime-only fields
     * (timer handle, callbacks, priv_data) are never persisted. */
    esp_schedule_persistent_t *persistent = (esp_schedule_persistent_t *)(blob_buffer);
    memset(persistent, 0, sizeof(*persistent));
    persistent->version = ESP_SCHEDULE_NVS_FORMAT_VERSION;
    persistent->struct_size = (uint16_t) sizeof(*persistent);
    persistent->trigger = schedule->trigger;
    persistent->validity = schedule->validity;
    strlcpy(persistent->name, schedule->name, sizeof(persistent->name));

    /* Add private data to end of buffer if saving private data. The buffer was
     * sized using the length reported by the first (sizing) on_save call, so the
     * second (data) call MUST report the same length. If it differs, abort the
     * save rather than truncate or overflow the buffer. */
    if (nvs_priv_data_callbacks.on_save != NULL && private_data_size > 0) {
        void *data = NULL;
        size_t data_size = 0;
        nvs_priv_data_callbacks.on_save(schedule->priv_data, &data, &data_size);
        if (data == NULL || data_size != private_data_size) {
            ESP_LOGE(TAG, "priv data size mismatch between on_save calls (%u vs %u); aborting save",
                     (unsigned)data_size, (unsigned)private_data_size);
            if (data != NULL) {
                free(data);
            }
            free(blob_buffer);
            nvs_close(nvs_handle);
            return ESP_FAIL;
        }
        memcpy(blob_buffer + sizeof(esp_schedule_persistent_t), data, data_size);
        free(data);
    }

    /* For a new schedule, read and validate the count BEFORE writing the blob,
     * so a rejected add (count read error, or count already at the limit) never
     * leaves an uncounted orphan blob that a later boot would resurrect or drop. */
    uint8_t schedule_count = 0;
    if (editing_schedule == false) {
        err = nvs_get_u8(nvs_handle, ESP_SCHEDULE_COUNT_KEY, &schedule_count);
        if (err != ESP_OK) {
            if (err == ESP_ERR_NVS_NOT_FOUND) {
                schedule_count = 0;
            } else {
                ESP_LOGE(TAG, "NVS get existing schedule count failed while adding schedule %s with error %d", schedule->name, err);
                free(blob_buffer);
                nvs_close(nvs_handle);
                return err;
            }
        }
        if (schedule_count == UINT8_MAX) {
            ESP_LOGE(TAG, "Schedule count at maximum (%u); not adding %s", UINT8_MAX, schedule->name);
            free(blob_buffer);
            nvs_close(nvs_handle);
            return ESP_ERR_NO_MEM;
        }
    }

    /* Store combined blob */
    err = nvs_set_blob(nvs_handle, schedule->name, blob_buffer, total_size);
    free(blob_buffer);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS set failed with error %d", err);
        nvs_close(nvs_handle);
        return err;
    }
    if (editing_schedule == false) {
        err = nvs_set_u8(nvs_handle, ESP_SCHEDULE_COUNT_KEY, (uint8_t)(schedule_count + 1));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS set failed for schedule count with error %d", err);
            /* Undo the blob we just wrote so the count and stored blobs stay
             * consistent (no uncounted orphan). */
            nvs_erase_key(nvs_handle, schedule->name);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
            return err;
        }
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Schedule %s added in NVS", schedule->name);
    return ESP_OK;
}

esp_err_t esp_schedule_nvs_remove_all(void)
{
    if (!nvs_enabled) {
        ESP_LOGD(TAG, "NVS not enabled. Not removing from NVS.");
        return ESP_ERR_INVALID_STATE;
    }
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open_from_partition(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed with error %d", err);
        return err;
    }
    err = nvs_erase_all(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS erase all keys failed with error %d", err);
        nvs_close(nvs_handle);
        return err;
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "All schedules removed from NVS");
    return ESP_OK;
}

esp_err_t esp_schedule_nvs_remove(esp_schedule_t *schedule)
{
    if (!nvs_enabled) {
        ESP_LOGD(TAG, "NVS not enabled. Not removing from NVS.");
        return ESP_ERR_INVALID_STATE;
    }
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open_from_partition(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed with error %d", err);
        return err;
    }

    /* Remove schedule blob (includes trigger data) */
    err = nvs_erase_key(nvs_handle, schedule->name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS erase key failed with error %d", err);
        nvs_close(nvs_handle);
        return err;
    }
    uint8_t schedule_count;
    err = nvs_get_u8(nvs_handle, ESP_SCHEDULE_COUNT_KEY, &schedule_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS get failed for schedule count with error %d", err);
        nvs_close(nvs_handle);
        return err;
    }
    /* Guard against underflow if the count key is already 0 (e.g. a prior power
     * loss desynced the count from the stored blobs). */
    if (schedule_count > 0) {
        schedule_count--;
    }
    err = nvs_set_u8(nvs_handle, ESP_SCHEDULE_COUNT_KEY, schedule_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS set failed for schedule count with error %d", err);
        nvs_close(nvs_handle);
        return err;
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Schedule %s removed from NVS", schedule->name);
    return ESP_OK;
}

static uint8_t esp_schedule_nvs_get_count(void)
{
    if (!nvs_enabled) {
        ESP_LOGD(TAG, "NVS not enabled. Not getting count from NVS.");
        return 0;
    }
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open_from_partition(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed with error %d", err);
        return 0;
    }
    uint8_t schedule_count;
    err = nvs_get_u8(nvs_handle, ESP_SCHEDULE_COUNT_KEY, &schedule_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS get failed for schedule count with error %d", err);
        nvs_close(nvs_handle);
        return 0;
    }
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Schedules in NVS: %d", schedule_count);
    return schedule_count;
}

/* Frozen on-flash layout of the pre-versioned (v1.x) schedule blob: v1 wrote a
 * raw esp_schedule_t via nvs_set_blob(..., sizeof(esp_schedule_t)). The first
 * field was the name, so a v1 blob's first byte is a printable name character,
 * never equal to ESP_SCHEDULE_NVS_FORMAT_VERSION; that plus the exact size is
 * how a v1 blob is recognized below.
 *
 * DO NOT EDIT to track the live esp_schedule_t. The embedded trigger and
 * validity types are the current ones only because their layouts are unchanged
 * since v1; the pointer fields are placeholders so the offsets and total size
 * match what v1 wrote on this ABI. */
typedef struct {
    char name[MAX_SCHEDULE_NAME_LEN + 1];
    esp_schedule_trigger_t trigger;    /* v1 stored a single trigger */
    uint32_t next_scheduled_time_diff; /* runtime-derived; ignored on load */
    void *timer;                       /* was TimerHandle_t; junk across reboot */
    void *trigger_cb;                  /* junk across reboot */
    void *timestamp_cb;                /* junk across reboot */
    void *priv_data;                   /* v1 stored the pointer, not the data */
    esp_schedule_validity_t validity;
} esp_schedule_legacy_v1_t;

/* Build a schedule from a pre-versioned (v1.x) blob, or return NULL if the blob
 * is not a recognizable v1 blob. In-memory migration only: the upgraded blob is
 * written back the next time the schedule is saved (create/edit), which avoids
 * mutating NVS while esp_schedule_nvs_get_all is iterating it. */
static esp_schedule_handle_t esp_schedule_migrate_from_v1(const char *nvs_key, const uint8_t *blob, size_t buf_size)
{
    if (buf_size != sizeof(esp_schedule_legacy_v1_t)) {
        ESP_LOGE(TAG, "Schedule %s blob is neither current format nor a known legacy layout (%u bytes); rejecting",
                 nvs_key, (unsigned)buf_size);
        return NULL;
    }
    const esp_schedule_legacy_v1_t *v1 = (const esp_schedule_legacy_v1_t *) blob;
    if (v1->trigger.type == ESP_SCHEDULE_TYPE_INVALID) {
        ESP_LOGE(TAG, "Schedule %s legacy blob has invalid trigger type; rejecting", nvs_key);
        return NULL;
    }

    esp_schedule_t *schedule = (esp_schedule_t *)calloc(1, sizeof(esp_schedule_t));
    if (schedule == NULL) {
        ESP_LOGE(TAG, "Could not allocate schedule while migrating %s", nvs_key);
        return NULL;
    }
    strlcpy(schedule->name, v1->name, sizeof(schedule->name));
    schedule->trigger = v1->trigger;
    schedule->validity = v1->validity;
    /* v1 only ever persisted the priv_data pointer value (meaningless across a
     * reboot), never its contents, so there is nothing to restore. Runtime
     * fields are already zeroed by calloc. */
    schedule->priv_data = NULL;

    ESP_LOGW(TAG, "Migrated schedule %s from legacy (pre-2.0) NVS format", nvs_key);
    return (esp_schedule_handle_t) schedule;
}

static esp_schedule_handle_t esp_schedule_nvs_get(const char *nvs_key)
{
    if (!nvs_enabled) {
        ESP_LOGD(TAG, "NVS not enabled. Not getting from NVS.");
        return NULL;
    }
    size_t buf_size;
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open_from_partition(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed with error %d", err);
        return NULL;
    }

    /* Get blob size */
    err = nvs_get_blob(nvs_handle, nvs_key, NULL, &buf_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS get failed with error %d", err);
        nvs_close(nvs_handle);
        return NULL;
    }

    /* Allocate buffer for entire blob */
    uint8_t *blob_buffer = (uint8_t *)malloc(buf_size);
    if (blob_buffer == NULL) {
        ESP_LOGE(TAG, "Could not allocate blob buffer");
        nvs_close(nvs_handle);
        return NULL;
    }

    /* Read entire blob */
    err = nvs_get_blob(nvs_handle, nvs_key, blob_buffer, &buf_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS get failed with error %d", err);
        nvs_close(nvs_handle);
        free(blob_buffer);
        return NULL;
    }
    nvs_close(nvs_handle);

    /* Validate the blob before trusting any field. A blob may be too short, in
     * an older/foreign format (e.g. after OTA), or written by a build with a
     * different layout (e.g. daylight support toggled, which changes
     * sizeof(esp_schedule_trigger_t)). Any of these would otherwise cause an
     * out-of-bounds read below. */
    size_t schedule_size = sizeof(esp_schedule_persistent_t);
    if (buf_size < schedule_size) {
        ESP_LOGE(TAG, "Schedule %s blob too small (%u < %u); rejecting", nvs_key, (unsigned)buf_size, (unsigned)schedule_size);
        free(blob_buffer);
        return NULL;
    }
    esp_schedule_persistent_t *persistent = (esp_schedule_persistent_t *)(blob_buffer);
    if (persistent->version != ESP_SCHEDULE_NVS_FORMAT_VERSION) {
        /* Not the current format. Attempt to migrate a pre-versioned (v1.x) blob
         * so schedules survive an OTA upgrade; any other/foreign blob is rejected
         * by the migrator (returns NULL). */
        ESP_LOGW(TAG, "Schedule %s blob is not current format (version byte %u); attempting legacy migration", nvs_key, persistent->version);
        esp_schedule_handle_t migrated = esp_schedule_migrate_from_v1(nvs_key, blob_buffer, buf_size);
        free(blob_buffer);
        return migrated;
    }
    if (persistent->struct_size != (uint16_t) schedule_size) {
        ESP_LOGE(TAG, "Schedule %s struct size %u != %u; rejecting", nvs_key, persistent->struct_size, (unsigned)schedule_size);
        free(blob_buffer);
        return NULL;
    }

    /* Allocate schedule structure */
    esp_schedule_t *schedule = (esp_schedule_t *)calloc(1, sizeof(esp_schedule_t));
    if (schedule == NULL) {
        ESP_LOGE(TAG, "Could not allocate schedule");
        free(blob_buffer);
        return NULL;
    }

    /* Reconstruct full schedule from persistent data. */
    strlcpy(schedule->name, persistent->name, sizeof(schedule->name));
    schedule->trigger = persistent->trigger;
    schedule->validity = persistent->validity;
    /* Runtime fields are zeroed by calloc: timer, trigger_cb, timestamp_cb, priv_data */

    /* Load private data if a loader is registered. schedule_size is bounded by
     * buf_size above, so this subtraction cannot underflow. */
    size_t data_len = buf_size - schedule_size;
    if (data_len > 0 && nvs_priv_data_callbacks.on_load != NULL) {
        void *data = (void *) blob_buffer + schedule_size;
        nvs_priv_data_callbacks.on_load(data, data_len, &schedule->priv_data);
    } else {
        schedule->priv_data = NULL;
    }

    free(blob_buffer);
    ESP_LOGI(TAG, "Schedule %s found in NVS", schedule->name);
    return (esp_schedule_handle_t) schedule;
}

esp_schedule_handle_t *esp_schedule_nvs_get_all(uint8_t *schedule_count)
{
    if (!nvs_enabled) {
        ESP_LOGD(TAG, "NVS not enabled. Not Initialising NVS.");
        return NULL;
    }

    *schedule_count = esp_schedule_nvs_get_count();
    if (*schedule_count == 0) {
        ESP_LOGI(TAG, "No Entries found in NVS");
        return NULL;
    }
    esp_schedule_handle_t *handle_list = (esp_schedule_handle_t *)calloc(*schedule_count, sizeof(esp_schedule_handle_t));
    if (handle_list == NULL) {
        ESP_LOGE(TAG, "Could not allocate schedule list");
        *schedule_count = 0;
        return NULL;
    }
    int handle_count = 0;

    nvs_entry_info_t nvs_entry;
    nvs_iterator_t nvs_iterator = NULL;
    esp_err_t err = nvs_entry_find(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, NVS_TYPE_BLOB, &nvs_iterator);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No entry found in NVS");
        free(handle_list);
        *schedule_count = 0;
        return NULL;
    }
    while (err == ESP_OK) {
        nvs_entry_info(nvs_iterator, &nvs_entry);
        /* handle_list is sized from the stored count key. If the iterator ever
         * yields more blobs than that count (e.g. a power loss between writing a
         * schedule blob and updating the count), stop rather than write past the
         * end of the array. */
        if (handle_count >= *schedule_count) {
            ESP_LOGW(TAG, "More schedule blobs than count (%u); ignoring extra key %s", *schedule_count, nvs_entry.key);
            break;
        }
        ESP_LOGI(TAG, "Found schedule in NVS with key: %s", nvs_entry.key);
        handle_list[handle_count] = esp_schedule_nvs_get(nvs_entry.key);
        if (handle_list[handle_count] != NULL) {
            /* Increase count only if nvs_get was successful */
            handle_count++;
        }
        err = nvs_entry_next(&nvs_iterator);
    }
    nvs_release_iterator(nvs_iterator);
    *schedule_count = handle_count;
    ESP_LOGI(TAG, "Found %d schedules in NVS", *schedule_count);
    return handle_list;
}

bool esp_schedule_nvs_is_enabled(void)
{
    return nvs_enabled;
}

void esp_schedule_nvs_free_loaded_priv_data(void *priv_data)
{
    if (priv_data != NULL && nvs_priv_data_callbacks.on_free != NULL) {
        nvs_priv_data_callbacks.on_free(priv_data);
    }
}

esp_err_t esp_schedule_nvs_init(char *nvs_partition, esp_schedule_priv_data_callbacks_t *priv_data_callbacks)
{
    /* Apply the callbacks unconditionally, even when NVS is already enabled: a
     * caller that initialized first with no callbacks (e.g. via the legacy
     * esp_schedule_init) and then re-initializes once its save/load handlers are
     * ready must not have those callbacks silently dropped. */
    if (priv_data_callbacks != NULL) {
        nvs_priv_data_callbacks = *priv_data_callbacks;
    }
    if (nvs_enabled) {
        ESP_LOGI(TAG, "NVS already enabled; callbacks updated");
        return ESP_OK;
    }
    if (nvs_partition) {
        esp_schedule_nvs_partition = strndup(nvs_partition, strlen(nvs_partition));
    } else {
        esp_schedule_nvs_partition = strndup("nvs", strlen("nvs"));
    }
    if (esp_schedule_nvs_partition == NULL) {
        ESP_LOGE(TAG, "Could not allocate nvs_partition");
        return ESP_ERR_NO_MEM;
    }
    nvs_enabled = true;
    return ESP_OK;
}
