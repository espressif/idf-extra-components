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

static const char *TAG = "esp_schedule_nvs";

#define ESP_SCHEDULE_NVS_NAMESPACE "schd"

/* Written by releases up to v1.4.x to track how many schedules were stored.
 * No longer maintained: the count is now derived by walking the namespace,
 * which cannot drift from reality and removes the underflow the old decrement
 * had. The key is still recognised here so a device upgrading from an older
 * build does not mistake the leftover value for a schedule. */
#define ESP_SCHEDULE_LEGACY_COUNT_KEY "schd_count"

static char *esp_schedule_nvs_partition = NULL;
static bool nvs_enabled = false;

#define NVS_OPS() (&g_esp_schedule_port.nvs)

static bool esp_schedule_nvs_is_reserved_key(const char *key)
{
    return strcmp(key, ESP_SCHEDULE_LEGACY_COUNT_KEY) == 0;
}

esp_err_t esp_schedule_nvs_add(esp_schedule_t *schedule)
{
    if (!nvs_enabled) {
        ESP_SCHEDULE_LOGD(TAG, "NVS not enabled. Not adding to NVS.");
        return ESP_ERR_INVALID_STATE;
    }
    esp_schedule_store_handle_t handle;
    esp_err_t err = NVS_OPS()->open(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, false, &handle);
    if (err != ESP_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS open failed with error %d", err);
        return err;
    }

    /* The in-memory timer handle means nothing in the next boot, and storing a
     * live pointer would leave the restored schedule referencing freed memory.
     * Persist the struct with that one field cleared.
     *
     * Write from a copy rather than clearing the field on the live object for
     * the duration of the write. A timer expiring inside that window reaches
     * esp_schedule_arm_timer() -> timer.start(&schedule->timer, ...), which
     * would see the NULL handle, take the create branch and install a second
     * timer - then be overwritten when the field was restored, leaking that
     * timer and leaving two of them dispatching the same schedule.
     *
     * priv_data goes the same way, and for the same reason as timer: it is the
     * caller's pointer into this boot's heap, meaningless in the next one, and
     * esp_schedule_get() hands it straight back to the application. Keeping it
     * out of flash means a restored schedule cannot carry a wild pointer. */
    esp_schedule_t stored = *schedule;
    stored.timer = NULL;
    stored.priv_data = NULL;
    err = NVS_OPS()->write(handle, schedule->name, &stored, sizeof(stored));

    if (err != ESP_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS write failed with error %d", err);
        NVS_OPS()->close(handle);
        return err;
    }
    NVS_OPS()->close(handle);
    ESP_SCHEDULE_LOGI(TAG, "Schedule %s added in NVS", schedule->name);
    return ESP_OK;
}

esp_err_t esp_schedule_nvs_remove_all(void)
{
    if (!nvs_enabled) {
        ESP_SCHEDULE_LOGD(TAG, "NVS not enabled. Not removing from NVS.");
        return ESP_ERR_INVALID_STATE;
    }
    esp_schedule_store_handle_t handle;
    esp_err_t err = NVS_OPS()->open(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, false, &handle);
    if (err != ESP_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS open failed with error %d", err);
        return err;
    }
    err = NVS_OPS()->erase(handle, NULL);
    if (err != ESP_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS erase all keys failed with error %d", err);
        NVS_OPS()->close(handle);
        return err;
    }
    NVS_OPS()->close(handle);
    ESP_SCHEDULE_LOGI(TAG, "All schedules removed from NVS");
    return ESP_OK;
}

esp_err_t esp_schedule_nvs_remove(esp_schedule_t *schedule)
{
    if (!nvs_enabled) {
        ESP_SCHEDULE_LOGD(TAG, "NVS not enabled. Not removing from NVS.");
        return ESP_ERR_INVALID_STATE;
    }
    esp_schedule_store_handle_t handle;
    esp_err_t err = NVS_OPS()->open(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, false, &handle);
    if (err != ESP_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS open failed with error %d", err);
        return err;
    }
    err = NVS_OPS()->erase(handle, schedule->name);
    if (err != ESP_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS erase key failed with error %d", err);
        NVS_OPS()->close(handle);
        return err;
    }
    NVS_OPS()->close(handle);
    ESP_SCHEDULE_LOGI(TAG, "Schedule %s removed from NVS", schedule->name);
    return ESP_OK;
}

/* Reads one stored schedule into a freshly allocated handle. */
static esp_schedule_handle_t esp_schedule_nvs_read_one(esp_schedule_store_handle_t handle, const char *key)
{
    size_t buf_size = 0;
    esp_err_t err = NVS_OPS()->read(handle, key, NULL, &buf_size);
    if (err != ESP_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS read of %s failed with error %d", key, err);
        return NULL;
    }
    /* Reject anything that is not exactly one esp_schedule_t. The blob is cast
     * straight to the struct, so a size written by a different build - a
     * changed Kconfig, another component version - would otherwise be misparsed
     * field for field with no diagnostic at all. */
    if (buf_size != sizeof(esp_schedule_t)) {
        ESP_SCHEDULE_LOGE(TAG, "Stored schedule %s is %u bytes, expected %u; ignoring it",
                          key, (unsigned)buf_size, (unsigned)sizeof(esp_schedule_t));
        return NULL;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)ESP_SCHEDULE_MALLOC(buf_size);
    if (schedule == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Could not allocate handle");
        return NULL;
    }
    err = NVS_OPS()->read(handle, key, schedule, &buf_size);
    if (err != ESP_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS read of %s failed with error %d", key, err);
        ESP_SCHEDULE_FREE(schedule);
        return NULL;
    }
    /* Defensive: blobs written before the store began clearing these fields
     * carry pointers from a previous boot. Clearing on the write side only
     * protects blobs written after the upgrade; anything already in flash still
     * has to be cleaned up here. priv_data matters most, since esp_schedule_get()
     * hands it straight back to the application. */
    schedule->timer = NULL;
    schedule->priv_data = NULL;
    ESP_SCHEDULE_LOGI(TAG, "Schedule %s found in NVS", schedule->name);
    return (esp_schedule_handle_t)schedule;
}

/* First pass over the namespace: how many schedules are actually stored. */
static bool esp_schedule_nvs_count_cb(const char *key, void *ctx)
{
    if (!esp_schedule_nvs_is_reserved_key(key)) {
        (*(uint8_t *)ctx)++;
    }
    return true;
}

typedef struct {
    esp_schedule_handle_t *list;
    esp_schedule_store_handle_t store;
    uint8_t capacity;
    uint8_t count;
} esp_schedule_nvs_load_ctx_t;

/* Second pass: load each stored schedule into the array the first pass sized. */
static bool esp_schedule_nvs_load_cb(const char *key, void *ctx)
{
    esp_schedule_nvs_load_ctx_t *load = (esp_schedule_nvs_load_ctx_t *)ctx;
    if (esp_schedule_nvs_is_reserved_key(key)) {
        return true;
    }
    /* The namespace could have grown between the two passes. Stop rather than
     * write past the array. */
    if (load->count >= load->capacity) {
        ESP_SCHEDULE_LOGW(TAG, "More schedules in NVS than counted; ignoring the rest");
        return false;
    }
    ESP_SCHEDULE_LOGI(TAG, "Found schedule in NVS with key: %s", key);
    esp_schedule_handle_t handle = esp_schedule_nvs_read_one(load->store, key);
    if (handle != NULL) {
        load->list[load->count++] = handle;
    }
    return true;
}

esp_schedule_handle_t *esp_schedule_nvs_get_all(uint8_t *schedule_count)
{
    *schedule_count = 0;
    if (!nvs_enabled) {
        ESP_SCHEDULE_LOGD(TAG, "NVS not enabled. Not Initialising NVS.");
        return NULL;
    }

    uint8_t stored = 0;
    esp_err_t err = NVS_OPS()->foreach_key(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE,
                                           esp_schedule_nvs_count_cb, &stored);
    if (err != ESP_OK || stored == 0) {
        ESP_SCHEDULE_LOGI(TAG, "No Entries found in NVS");
        return NULL;
    }

    esp_schedule_handle_t *handle_list =
        (esp_schedule_handle_t *)ESP_SCHEDULE_MALLOC(sizeof(esp_schedule_handle_t) * stored);
    if (handle_list == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Could not allocate schedule list");
        return NULL;
    }

    esp_schedule_nvs_load_ctx_t load = { .list = handle_list, .capacity = stored, .count = 0 };
    err = NVS_OPS()->open(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE, true, &load.store);
    if (err != ESP_OK) {
        ESP_SCHEDULE_LOGE(TAG, "NVS open failed with error %d", err);
        ESP_SCHEDULE_FREE(handle_list);
        return NULL;
    }
    err = NVS_OPS()->foreach_key(esp_schedule_nvs_partition, ESP_SCHEDULE_NVS_NAMESPACE,
                                 esp_schedule_nvs_load_cb, &load);
    NVS_OPS()->close(load.store);

    if (err != ESP_OK || load.count == 0) {
        ESP_SCHEDULE_FREE(handle_list);
        return NULL;
    }
    *schedule_count = load.count;
    ESP_SCHEDULE_LOGI(TAG, "Found %d schedules in NVS", *schedule_count);
    return handle_list;
}

bool esp_schedule_nvs_is_enabled(void)
{
    return nvs_enabled;
}

esp_err_t esp_schedule_nvs_init(char *nvs_partition)
{
    if (nvs_enabled) {
        ESP_SCHEDULE_LOGI(TAG, "NVS already enabled");
        return ESP_OK;
    }
    /* The port may legitimately supply no storage, in which case persistence is
     * unavailable no matter what the caller asked for. esp_schedule_port_install()
     * has already checked that the storage operations are either all present or
     * all absent, so testing one of them is enough. */
    if (NVS_OPS()->open == NULL) {
        ESP_SCHEDULE_LOGW(TAG, "Port supplies no storage; schedules will not persist");
        return ESP_ERR_NOT_SUPPORTED;
    }
    /* Copied through the port allocator so every allocation stays on one heap. */
    const char *partition = nvs_partition ? nvs_partition : "nvs";
    size_t partition_len = strlen(partition);
    esp_schedule_nvs_partition = (char *)ESP_SCHEDULE_MALLOC(partition_len + 1);
    if (esp_schedule_nvs_partition == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Could not allocate nvs_partition");
        return ESP_ERR_NO_MEM;
    }
    memcpy(esp_schedule_nvs_partition, partition, partition_len + 1);
    nvs_enabled = true;
    return ESP_OK;
}
