/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
#include "esp_blockdev.h"
#endif // (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))

#include "esp_ext_part_tables.h"
#include "esp_mbr.h"

#if __has_include(<bsd/sys/queue.h>)
#include <bsd/sys/queue.h>
#else
#include "sys/queue.h"
#endif

uint64_t esp_ext_part_bytes_to_sector_count(uint64_t total_bytes, esp_ext_part_sector_size_t sector_size)
{
    if (sector_size == ESP_EXT_PART_SECTOR_SIZE_UNKNOWN) {
        return 0; // Avoid division by zero
    }
    // Ceiling division for integers: (a + b - 1) / b
    return ((total_bytes + (uint64_t) sector_size - 1) / (uint64_t) sector_size);
}

uint64_t esp_ext_part_sector_count_to_bytes(uint64_t sector_count, esp_ext_part_sector_size_t sector_size)
{
    return sector_count * (uint64_t) sector_size;
}

esp_err_t esp_ext_part_list_deinit(esp_ext_part_list_t *part_list)
{
    if (part_list == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ext_part_list_item_t *it = NULL;
    esp_ext_part_list_item_t *tmp = NULL;
    SLIST_FOREACH_SAFE(it, &part_list->head, next, tmp) {
        SLIST_REMOVE(&part_list->head, it, esp_ext_part_list_item_, next);
        free(it->info.label); // Deep free the label if it was allocated
        free(it); // Free the item itself
    }
    memset(part_list, 0, sizeof(esp_ext_part_list_t)); // Reset the part_list structure
    return ESP_OK;
}

esp_err_t esp_ext_part_list_insert(esp_ext_part_list_t *part_list, esp_ext_part_list_item_t *item)
{
    if (part_list == NULL || item == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ext_part_list_item_t *_item = (esp_ext_part_list_item_t *) malloc(sizeof(esp_ext_part_list_item_t));
    if (_item == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memcpy(_item, item, sizeof(esp_ext_part_list_item_t)); // Copy the item
    if (_item->info.label != NULL) {
        _item->info.label = strdup(item->info.label); // Deep copy the label
        if (_item->info.label == NULL) {
            free(_item);
            return ESP_ERR_NO_MEM;
        }
    }

    esp_ext_part_list_item_t *it = NULL;
    esp_ext_part_list_item_t *last = NULL;
    SLIST_FOREACH(it, &part_list->head, next) {
        last = it;
    }
    if (last == NULL) {
        SLIST_INSERT_HEAD(&part_list->head, _item, next);
    } else {
        SLIST_INSERT_AFTER(last, _item, next);
    }
    return ESP_OK;
}

esp_err_t esp_ext_part_list_deep_copy(esp_ext_part_list_t *dst, esp_ext_part_list_t *src)
{
    if (dst == NULL || src == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(dst, src, sizeof(esp_ext_part_list_t)); // Copy the structure
    memset(&dst->head, 0, sizeof(dst->head)); // Reset the head of the destination list

    esp_err_t err;
    esp_ext_part_list_item_t *it = NULL;
    SLIST_FOREACH(it, &src->head, next) {
        err = esp_ext_part_list_insert(dst, it); // Insert copies the item from src to dst
        if (err != ESP_OK) {
            esp_ext_part_list_deinit(dst);
            return err;
        }
    }
    return ESP_OK;
}

esp_ext_part_list_item_t *esp_ext_part_list_item_head(esp_ext_part_list_t *part_list)
{
    if (part_list == NULL) {
        return NULL;
    }
    return SLIST_FIRST(&part_list->head);
}

esp_ext_part_list_item_t *esp_ext_part_list_item_next(esp_ext_part_list_item_t *item)
{
    if (item == NULL) {
        return NULL;
    }
    return SLIST_NEXT(item, next);
}

esp_err_t esp_ext_part_list_signature_get(esp_ext_part_list_t *part_list, void *signature)
{
    if (part_list == NULL || signature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t out = 0;
    switch (part_list->signature.type) {
    case ESP_EXT_PART_LIST_SIGNATURE_MBR:
        out = (uint32_t) part_list->signature.data[0];
        memcpy(signature, &out, sizeof(uint32_t));
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED; // Unsupported signature type
    }
    return ESP_OK;
}

esp_err_t esp_ext_part_list_signature_set(esp_ext_part_list_t *part_list, const void *signature, esp_ext_part_signature_type_t type)
{
    if (part_list == NULL || signature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    part_list->signature.type = type;
    switch (type) {
    case ESP_EXT_PART_LIST_SIGNATURE_MBR:
        part_list->signature.data[0] = *((const uint32_t *) signature);
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED; // Unsupported signature type
    }
    return ESP_OK;
}

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
esp_err_t esp_ext_part_list_bdl_read(esp_blockdev_handle_t handle, esp_ext_part_list_t *part_list, esp_ext_part_signature_type_t type, void *extra_args)
{
    if (handle == NULL || part_list == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    uint8_t *buf = NULL;

    switch (type) {
    case ESP_EXT_PART_LIST_SIGNATURE_MBR:
        buf = malloc(MBR_SIZE);
        if (buf == NULL) {
            return ESP_ERR_NO_MEM;
        }

        err = handle->ops->read(handle, buf, MBR_SIZE, 0, MBR_SIZE);
        if (err != ESP_OK) {
            free(buf);
            return err;
        }

        err = esp_mbr_parse(buf, part_list, (esp_mbr_parse_extra_args_t *) extra_args);
        free(buf);
        break;

    default:
        err = ESP_ERR_NOT_SUPPORTED; // Unsupported signature type
        break;
    }

    return err;
}

esp_err_t esp_ext_part_list_bdl_write(esp_blockdev_handle_t handle, esp_ext_part_list_t *part_list, esp_ext_part_signature_type_t type, void *extra_args)
{
    if (handle == NULL || part_list == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    uint8_t *buf = NULL;

    switch (type) {
    case ESP_EXT_PART_LIST_SIGNATURE_MBR:
        buf = malloc(MBR_SIZE);
        if (buf == NULL) {
            return ESP_ERR_NO_MEM;
        }

        err = esp_mbr_generate((mbr_t *) buf, part_list, (esp_mbr_generate_extra_args_t *) extra_args);
        if (err != ESP_OK) {
            free(buf);
            return err;
        }

        err = handle->ops->write(handle, buf, 0, MBR_SIZE);
        free(buf);
        break;

    default:
        err = ESP_ERR_NOT_SUPPORTED; // Unsupported signature type
        break;
    }

    return err;
}
#endif // (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
