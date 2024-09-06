/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <errno.h>
#include <esp_log.h>
#include <esp_err.h>
#include "esp_partition.h"
#include "esp_custom_part_ota.h"
#include "esp_ota_ops.h"
#include "sys/param.h"
#include "nvs.h"

static const char *TAG = "esp_custom_part_ota";

#define BACKUP_STORAGE_NAMESPACE "esp_custom_ota"
#define BACKUP_STORAGE_DATA_LEN "backup_len"

typedef struct {
    const esp_partition_t *update_partition;
    const esp_partition_t *backup_partition;
    size_t wrote_size;
    size_t backup_len;
    bool need_erase;
} esp_custom_part_ota_t;

static esp_err_t set_nvs_backup_length(uint32_t backup_length)
{
    nvs_handle_t backup_info_handle = 0;
    esp_err_t ret = 0;
    ret = nvs_open(BACKUP_STORAGE_NAMESPACE, NVS_READWRITE, &backup_info_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store backup information: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = nvs_set_u32(backup_info_handle, BACKUP_STORAGE_DATA_LEN, backup_length);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store backup information: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = nvs_commit(backup_info_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store backup information: %s", esp_err_to_name(ret));
        return ret;
    }
    nvs_close(backup_info_handle);
    return ret;
}

esp_custom_part_ota_handle_t esp_custom_part_ota_begin(esp_custom_part_ota_cfg_t config)
{
    if (!config.update_partition) {
        ESP_LOGE(TAG, "esp_custom_part_ota_begin: Invalid argument");
        return NULL;
    }
    if (config.update_partition->type == ESP_PARTITION_TYPE_APP) {
        ESP_LOGE(TAG, "esp_custom_part_ota_begin: Partition of type APP not supported");
        return NULL;
    }
    esp_custom_part_ota_t *ctx = calloc(1, sizeof(esp_custom_part_ota_t));
    if (!ctx) {
        ESP_LOGE(TAG, "esp_custom_part_ota_begin: Could not allocate memory for handle");
        return NULL;
    }
    ctx->update_partition = config.update_partition;

    ctx->backup_partition = config.backup_partition;
    if (ctx->backup_partition == esp_ota_get_running_partition()) {
        ESP_LOGE(TAG, "esp_custom_part_ota_begin: Backup partition cannot be running partition");
        free(ctx);
        return NULL;
    }
    if (ctx->backup_partition == NULL) {
        ESP_LOGI(TAG, "esp_custom_part_ota_begin: No backup partition supplied, setting passive app partition as backup");
        ctx->backup_partition = esp_ota_get_next_update_partition(NULL);
        if (!ctx->backup_partition) {
            ESP_LOGW(TAG, "No backup partition found");
        }
    }
    ctx->need_erase = 1;
    ctx->backup_len = 0;

    esp_custom_part_ota_handle_t handle = (esp_custom_part_ota_handle_t)ctx;
    return handle;
}

esp_err_t esp_custom_part_ota_write(esp_custom_part_ota_handle_t handle, const void *data, size_t size)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_custom_part_ota_t *ctx = (esp_custom_part_ota_t *)handle;
    if (!ctx->update_partition) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ESP_FAIL;
    if (ctx->need_erase) {
        ret = esp_partition_erase_range(ctx->update_partition, 0, ctx->update_partition->size);
        if (ret == ESP_OK) {
            ctx->need_erase = 0;
            ESP_LOGI(TAG, "Successfully erased update partition");
        } else {
            return ret;
        }
    }
    ret = esp_partition_write(ctx->update_partition, ctx->wrote_size, data, size);
    if (ret == ESP_OK) {
        ctx->wrote_size += size;
    }
    return ret;
}

esp_err_t esp_custom_part_ota_end(esp_custom_part_ota_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_custom_part_ota_t *ctx = (esp_custom_part_ota_t *)handle;
    if (ctx->wrote_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (ctx->backup_partition) {
        esp_err_t ret = set_nvs_backup_length(0);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    free(ctx);
    return ESP_OK;
}

esp_err_t esp_custom_part_ota_abort(esp_custom_part_ota_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_custom_part_ota_t *ctx = (esp_custom_part_ota_t *)handle;
    free(ctx);
    return ESP_OK;
}

esp_err_t esp_custom_part_ota_partition_backup(esp_custom_part_ota_handle_t handle, size_t backup_size)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_custom_part_ota_t *ctx = (esp_custom_part_ota_t *)handle;
    if (!ctx->backup_partition) {
        ESP_LOGE(TAG, "Backup partition not set. Cannot backup");
        return ESP_FAIL;
    }
    ctx->backup_len = (backup_size == 0) ? ctx->update_partition->size : backup_size;
    if (ctx->backup_len > ctx->backup_partition->size) {
        ESP_LOGE(TAG, "Backup partition size smaller than data to be backed up");
        return ESP_FAIL;
    }
    size_t block_size = 4096;
    uint8_t *data = calloc(block_size, sizeof(uint8_t));
    if (!data) {
        ESP_LOGE(TAG, "Could not allocate memory for data");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = esp_partition_erase_range(ctx->backup_partition, 0, ctx->backup_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase the backup partition");
        goto end;
    }

    if (ctx->backup_len > 0) {
        size_t copy_size = 0;
        while (copy_size < ctx->backup_len) {
            size_t data_size = MIN(block_size, ctx->backup_len - copy_size);
            ret = esp_partition_read(ctx->update_partition, copy_size, data, data_size);
            if (ret != ESP_OK) {
                goto end;
            }
            ret = esp_partition_write(ctx->backup_partition, copy_size, data, data_size);
            if (ret != ESP_OK) {
                goto end;
            }
            copy_size += data_size;
        }
    }
end:
    free(data);
    if (ret == ESP_OK) {
        // Store backup data information in the default NVS partition.
        ret = set_nvs_backup_length(ctx->backup_len);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set nvs backup length to zero");
        }
        ret = esp_partition_erase_range(ctx->update_partition, 0, ctx->update_partition->size);
        if (ret == ESP_OK) {
            ctx->need_erase = 0;
            ESP_LOGI(TAG, "Successfully erased update partition");
        }
    }
    return ret;
}

esp_err_t esp_custom_part_ota_partition_restore(esp_custom_part_ota_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_custom_part_ota_t *ctx = (esp_custom_part_ota_t *)handle;
    if (!ctx->backup_partition || !ctx->update_partition) {
        ESP_LOGE(TAG, "Partition(s) not set. Cannot restore");
        return ESP_FAIL;
    }
    esp_err_t ret = ESP_FAIL;

    // Check if valid backup is present in the backup partition by reading backup information from the default NVS partition.
    if (ctx->backup_len == 0) {
        nvs_handle_t backup_info_handle = 0;
        ret = nvs_open(BACKUP_STORAGE_NAMESPACE, NVS_READWRITE, &backup_info_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to fetch backup information from NVS: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = nvs_get_u32(backup_info_handle, BACKUP_STORAGE_DATA_LEN, (uint32_t *)(&(ctx->backup_len)));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to fetch backup information from NVS: %s", esp_err_to_name(ret));
            return ret;
        }
        nvs_close(backup_info_handle);
    }
    if (ctx->backup_len == 0) {
        ESP_LOGI(TAG, "No backup present in the backup partition. Nothing to restore");
        return ESP_OK;
    }
    if (ctx->backup_len > 0) {
        ret = esp_partition_erase_range(ctx->update_partition, 0, ctx->update_partition->size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase update partition %s", esp_err_to_name(ret));
            goto end;
        }
        size_t block_size = 4096;
        uint8_t *data = calloc(block_size, sizeof(uint8_t));
        if (!data) {
            ESP_LOGE(TAG, "Could not allocate memory for data");
            return ESP_ERR_NO_MEM;
        }
        size_t copy_size = 0;
        while (copy_size < ctx->backup_len) {
            size_t data_size = MIN(block_size, ctx->backup_len - copy_size);
            ret = esp_partition_read(ctx->backup_partition, copy_size, data, data_size);
            if (ret != ESP_OK) {
                free(data);
                goto end;
            }
            ret = esp_partition_write(ctx->update_partition, copy_size, data, data_size);
            if (ret != ESP_OK) {
                free(data);
                goto end;
            }
            copy_size += data_size;
        }
        free(data);
    }
end:
    if (ret == ESP_OK) {
        ret = set_nvs_backup_length(0);
    }
    return ret;
}
