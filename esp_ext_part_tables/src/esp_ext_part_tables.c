/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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

static const char *TAG = "esp_ext_part";

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

esp_err_t esp_ext_part_list_insert(esp_ext_part_list_t *part_list, const esp_ext_part_list_item_t *item)
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

esp_err_t esp_ext_part_list_deep_copy(esp_ext_part_list_t *dst, const esp_ext_part_list_t *src)
{
    if (dst == NULL || src == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Overwriting a destination that still holds items would drop the only pointers to
    // them (and to their labels), so require an empty list, like esp_mbr_parse does.
    if (!SLIST_EMPTY(&dst->head)) {
        ESP_LOGE(TAG, "Destination partition list is not empty, call esp_ext_part_list_deinit() before copying into it");
        return ESP_ERR_INVALID_STATE;
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

esp_ext_part_list_item_t *esp_ext_part_list_next_matching(esp_ext_part_list_item_t *from, const esp_ext_part_list_t *list, const esp_ext_part_match_t *matcher)
{
    if (matcher == NULL || matcher->fn == NULL) {
        return NULL;
    }
    esp_ext_part_list_item_t *it;
    if (from != NULL) {
        it = SLIST_NEXT(from, next);
    } else if (list != NULL) {
        it = SLIST_FIRST(&list->head);
    } else {
        return NULL;
    }

    for (; it != NULL; it = SLIST_NEXT(it, next)) {
        if (matcher->fn(&it->info, matcher->ctx)) {
            return it;
        }
    }
    return NULL;
}

esp_err_t esp_ext_part_list_signature_get(const esp_ext_part_list_t *part_list, esp_ext_part_list_signature_t *signature)
{
    if (part_list == NULL || signature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (part_list->signature.type) {
    case ESP_EXT_PART_LIST_SIGNATURE_MBR:
        *signature = part_list->signature;
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED; // Unsupported signature type
    }
    return ESP_OK;
}

esp_err_t esp_ext_part_list_signature_set(esp_ext_part_list_t *part_list, const esp_ext_part_list_signature_t *signature)
{
    if (part_list == NULL || signature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (signature->type) {
    case ESP_EXT_PART_LIST_SIGNATURE_MBR:
        part_list->signature = *signature;
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED; // Unsupported signature type
    }
    return ESP_OK;
}

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
esp_err_t esp_ext_part_probe(esp_blockdev_handle_t handle, esp_ext_part_signature_type_t *out_type)
{
    if (handle == NULL || out_type == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *buf = malloc(ESP_MBR_SIZE);
    if (buf == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = handle->ops->read(handle, buf, ESP_MBR_SIZE, 0, ESP_MBR_SIZE);
    if (err != ESP_OK) {
        free(buf);
        return err;
    }

    // The first sector of a partitioned medium is always an MBR. On a GPT disk it is a
    // protective MBR whose single entry has type 0xEE, which is what distinguishes the
    // two formats here.
    const esp_mbr_t *mbr = (const esp_mbr_t *) buf;
    if (mbr->boot_signature != ESP_MBR_SIGNATURE) {
        ESP_LOGD(TAG, "No MBR boot signature, no known partition table");
        free(buf);
        return ESP_ERR_NOT_FOUND;
    }

    *out_type = ESP_EXT_PART_LIST_SIGNATURE_MBR;
    for (int i = 0; i < ESP_MBR_MAX_PARTITION_COUNT; i++) {
        if (mbr->partition_table[i].type == ESP_MBR_PARTITION_TYPE_GPT_PROTECTIVE) {
            *out_type = ESP_EXT_PART_LIST_SIGNATURE_GPT;
            break;
        }
    }

    free(buf);
    return ESP_OK;
}
#endif // (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
