/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_random.h"

#include "esp_ext_part_tables.h"
#include "esp_mbr.h"
#include "esp_mbr_utils.h"

static const char *TAG = "esp_mbr";

static void ext_part_list_item_do_extra(esp_ext_part_list_item_t *item, mbr_partition_t *partition)
{
    // This function is for any extra actions that might be needed for specific partition types.
    // It can be used to set flags, perform additional operations or checks if needed.

    switch (item->info.type) { // Parsed type
    case ESP_EXT_PART_TYPE_LITTLEFS:
        item->info.flags |= ESP_EXT_PART_FLAG_EXTRA; // Set the extra flag to indicate that this partition has extra information
        item->info.extra = (uint64_t) esp_mbr_chs_arr_val_get(partition->chs_start); // Put LittleFS block size which was stored in `chs_start` to `extra` field
        break;
    default:
        break;
    }
}

esp_err_t esp_mbr_parse(void *mbr_buf,
                        esp_ext_part_list_t *part_list,
                        esp_mbr_parse_extra_args_t *extra_args)
{
    if (mbr_buf == NULL || part_list == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    mbr_t *mbr = (mbr_t *) mbr_buf;
    // Check MBR signature
    if (mbr->boot_signature != MBR_SIGNATURE) {
        ESP_LOGE(TAG, "MBR signature not found");
        return ESP_ERR_NOT_FOUND;
    }

    // Set defaults
    part_list->sector_size = ESP_EXT_PART_SECTOR_SIZE_512B; // Default sector size
    bool (*f_parse_supported_partition_types)(uint8_t, uint8_t *) = esp_mbr_parse_default_supported_partition_types;

    // Load extra arguments if provided
    if (extra_args) {
        if (extra_args->sector_size != ESP_EXT_PART_SECTOR_SIZE_UNKNOWN) {
            part_list->sector_size = extra_args->sector_size; // Use the sector size hint from extra_args
        }
        if (extra_args->esp_mbr_parse_custom_supported_partition_types) {
            f_parse_supported_partition_types = extra_args->esp_mbr_parse_custom_supported_partition_types; // Use a custom function for supported partition types
        }
    }

    esp_err_t err = ESP_OK;
    if (mbr->copy_protected == MBR_COPY_PROTECTED) {
        part_list->flags |= ESP_EXT_PART_LIST_FLAG_READ_ONLY;
    }

    err = esp_ext_part_list_signature_set(part_list, &mbr->disk_signature, ESP_EXT_PART_LIST_SIGNATURE_MBR);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set partition list (disk) signature");
        return err;
    }

    mbr_partition_t *partition;
    for (int i = 0; i < 4; i++) {
        partition = (mbr_partition_t *) &mbr->partition_table[i];

        // Check if the partition entry is empty and if so, skip it
        if (partition->type == 0x00) {
            break; // No more partitions, exit the loop (MBR partition table cannot have holes in it)
        }

        // If the partition entry is not supported, skip it as well
        uint8_t parsed_type = ESP_EXT_PART_TYPE_NONE;
        bool is_supported = f_parse_supported_partition_types(partition->type, &parsed_type);
        if (!is_supported) {
            continue;
        }

        // Create a new partition item and populate it with the partition info
        esp_ext_part_list_item_t item = {
            .info = {
                .address = esp_ext_part_sector_count_to_bytes((uint64_t) partition->lba_start, part_list->sector_size),
                .size = esp_ext_part_sector_count_to_bytes((uint64_t) partition->sector_count, part_list->sector_size),
                .extra = 0,
                .label = NULL, // MBR does not have labels
                .flags = ESP_EXT_PART_FLAG_NONE,
                .type = parsed_type,
            }
        };

        if (partition->status == MBR_PARTITION_STATUS_ACTIVE) {
            item.info.flags |= ESP_EXT_PART_FLAG_ACTIVE;
        }

        // Set the flags or extra field based on the partition type or do any extra actions needed
        ext_part_list_item_do_extra(&item, partition);

        // Add the partition info to the output table
        err = esp_ext_part_list_insert(part_list, &item);
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "Failed to add partition info to list");
            return err;
        }
    }
    return ESP_OK;
}

static bool mbr_partition_fill(mbr_partition_t *partition, esp_ext_part_list_item_t *item)
{
    uint32_t lba_start = partition->lba_start;
    uint32_t lba_end = lba_start - 1 + partition->sector_count;

    switch (item->info.type) {
    case ESP_EXT_PART_TYPE_FAT12:
    case ESP_EXT_PART_TYPE_FAT16:
    case ESP_EXT_PART_TYPE_FAT32:
        // Set CHS values based on LBA start and end
        esp_mbr_lba_to_chs_arr(partition->chs_start, lba_start);
        esp_mbr_lba_to_chs_arr(partition->chs_end, lba_end);
        break;
    case ESP_EXT_PART_TYPE_LITTLEFS:
        // Use `chs_start` to store LittleFS block size (if stored in `extra` field)
        if (item->info.extra != 0) {
            // If the extra flag is set, use the extra field to store the LittleFS block size
            esp_mbr_chs_arr_val_set(partition->chs_start, (uint32_t) item->info.extra);

            if (!(item->info.flags & ESP_EXT_PART_FLAG_EXTRA)) {
                // If the extra flag is not set but the extra field is set, log a warning
                ESP_LOGW(TAG, "LittleFS partition with extra field set but extra flag was not set");
            }
        } else {
            ESP_LOGE(TAG, "LittleFS partition with 0xC3 type without any block size value in `extra` field");
            return false; // Error
        }
        break;
    default:
        break;
    }
    return true; // OK
}

esp_err_t esp_mbr_partition_set(mbr_t *mbr, uint8_t partition_index, esp_ext_part_list_item_t *item, esp_mbr_generate_extra_args_t *extra_args)
{
    if (mbr == NULL || partition_index >= 4 || item == NULL || extra_args == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Set defaults
    mbr_partition_t *partition = &mbr->partition_table[partition_index];
    uint8_t (*f_generate_supported_partition_types)(uint8_t) = esp_mbr_generate_default_supported_partition_types;

    // Load extra arguments if provided
    if (extra_args->esp_mbr_generate_custom_supported_partition_types) {
        f_generate_supported_partition_types = extra_args->esp_mbr_generate_custom_supported_partition_types;  // Use a custom function for supported partition types
    }

    // Check if the partition entry is empty and if so, skip it
    if (item->info.type == ESP_EXT_PART_TYPE_NONE) {
        memset(partition, 0, sizeof(mbr_partition_t));
        return ESP_OK; // No partition to set
    }

    // Check if we have enough space in the MBR partition table
    uint64_t first_sector_address = esp_ext_part_bytes_to_sector_count(item->info.address, extra_args->sector_size);
    uint64_t sector_count = esp_ext_part_bytes_to_sector_count(item->info.size, extra_args->sector_size);
    if (first_sector_address > UINT32_MAX || sector_count > UINT32_MAX) {
        ESP_LOGE(TAG, "Partition address or size exceeds 32-bit limit of MBR");
        return ESP_ERR_NOT_SUPPORTED; // Address or size too large for MBR
    }

    // Set the partition info
    if (item->info.flags & ESP_EXT_PART_FLAG_ACTIVE) {
        partition->status = MBR_PARTITION_STATUS_ACTIVE;
    }
    partition->lba_start = esp_mbr_lba_align((uint32_t) first_sector_address, extra_args->sector_size, extra_args->alignment);
    partition->sector_count = (uint32_t) sector_count;
    partition->type = f_generate_supported_partition_types(item->info.type);

    if (mbr_partition_fill(partition, item) == false) {
        return ESP_ERR_INVALID_STATE; // Error filling partition
    }

    return ESP_OK;
}

esp_err_t esp_mbr_generate(mbr_t *mbr,
                           esp_ext_part_list_t *part_list,
                           esp_mbr_generate_extra_args_t *extra_args)
{
    if (mbr == NULL || part_list == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = ESP_OK;

    // Set default arguments for MBR generation
    esp_mbr_generate_extra_args_t args = {
        .sector_size = part_list->sector_size != ESP_EXT_PART_SECTOR_SIZE_UNKNOWN ? part_list->sector_size : ESP_EXT_PART_SECTOR_SIZE_512B, // Default sector size
        .alignment = ESP_EXT_PART_ALIGN_1MiB, // Default alignment
        .keep_signature = false, // Default is to generate a new disk signature
    };

    // Load extra arguments if provided
    if (extra_args) {
        if (extra_args->sector_size != ESP_EXT_PART_SECTOR_SIZE_UNKNOWN) {
            args.sector_size = extra_args->sector_size;
        }
        if (extra_args->alignment != ESP_EXT_PART_ALIGN_NONE) {
            args.alignment = extra_args->alignment;
        }
        args.keep_signature = extra_args->keep_signature;
        if (extra_args->esp_mbr_generate_custom_supported_partition_types) {
            args.esp_mbr_generate_custom_supported_partition_types = extra_args->esp_mbr_generate_custom_supported_partition_types;
        }
    }

    mbr->boot_signature = MBR_SIGNATURE;
    if (args.keep_signature) {
        // Use the disk signature from the partition list
        err = esp_ext_part_list_signature_get(part_list, &mbr->disk_signature);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get disk signature from partition list");
            return err;
        }
    } else {
        mbr->disk_signature = esp_random();
    }

    if (part_list->flags & ESP_EXT_PART_LIST_FLAG_READ_ONLY) {
        mbr->copy_protected = MBR_COPY_PROTECTED;
    }

    esp_ext_part_list_item_t *it = NULL;
    int i = 0;
    SLIST_FOREACH(it, &part_list->head, next) {
        err = esp_mbr_partition_set(mbr, i, it, &args);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set partition %d: %s", i, esp_err_to_name(err));
            return err; // Error setting partition
        }
        i += 1;
    }

    return ESP_OK;
}

esp_err_t esp_mbr_remove_gaps_between_partiton_entries(mbr_t *mbr)
{
    if (mbr == NULL) {
        return ESP_ERR_INVALID_ARG; // Invalid MBR pointer
    }

    // Iterate through the partition table and remove gaps
    mbr_partition_t *partition;
    uint8_t gap_index = 0; // Next index to fill
    for (int i = 0; i < 4; i++) {
        partition = &mbr->partition_table[i];
        if (partition->type == 0x00) {
            continue; // Skip empty entries
        }
        if (gap_index != i) {
            // Move the partition to the next available index
            memcpy(&mbr->partition_table[gap_index], partition, sizeof(mbr_partition_t));
            memset(partition, 0, sizeof(mbr_partition_t)); // Clear the old entry
        }
        gap_index++;
    }

    return ESP_OK;
}
