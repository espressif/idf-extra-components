/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <inttypes.h>
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
    const esp_ext_part_match_t *matcher = NULL; // Optional parse-time filter (NULL / NULL fn = keep all)

    // Load extra arguments if provided
    if (extra_args) {
        if (extra_args->sector_size != ESP_EXT_PART_SECTOR_SIZE_UNKNOWN) {
            part_list->sector_size = extra_args->sector_size; // Use the sector size hint from extra_args
        }
        if (extra_args->esp_mbr_parse_custom_supported_partition_types) {
            f_parse_supported_partition_types = extra_args->esp_mbr_parse_custom_supported_partition_types; // Use a custom function for supported partition types
        }
        matcher = &extra_args->match; // .fn == NULL keeps all recognized partitions
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

        // Resolve the partition type. The bool return is not used to gate insertion;
        // filtering is done by the optional `match` predicate below.
        uint8_t parsed_type = ESP_EXT_PART_TYPE_NONE;
        (void) f_parse_supported_partition_types(partition->type, &parsed_type);
        if (parsed_type == ESP_EXT_PART_TYPE_NONE) {
            // Unknown/extended type: cannot be represented, so a regenerated table
            // would differ from the source.
            part_list->flags |= ESP_EXT_PART_LIST_FLAG_LOSSY;
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

        // Apply the optional parse-time filter on the fully-populated info. A rejected
        // partition is dropped, which makes a regenerated table differ from the source.
        if (matcher != NULL && matcher->fn != NULL && !matcher->fn(&item.info, matcher->ctx)) {
            part_list->flags |= ESP_EXT_PART_LIST_FLAG_LOSSY;
            continue;
        }

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

    uint32_t aligned_start = esp_mbr_lba_align((uint32_t) first_sector_address, extra_args->sector_size, extra_args->alignment);

    if (aligned_start != (uint32_t) first_sector_address) {
        // Alignment moved the partition start; apply the configured policy.
        switch (extra_args->align_policy) {
        case ESP_EXT_PART_ALIGN_POLICY_KEEP_SIZE:
            // Default: keep the requested size as the length from the aligned start (matches fdisk/parted).
            break;
        case ESP_EXT_PART_ALIGN_POLICY_REJECT:
            ESP_LOGE(TAG, "Partition %u start (sector %" PRIu32 ") is not aligned and align_policy is REJECT",
                     partition_index, (uint32_t) first_sector_address);
            return ESP_ERR_INVALID_ARG;
        case ESP_EXT_PART_ALIGN_POLICY_PRESERVE_END: {
            // Shrink the size so the end stays at the originally requested address + size.
            uint64_t orig_end = first_sector_address + sector_count; // exclusive end, in sectors
            if ((uint64_t) aligned_start >= orig_end) {
                ESP_LOGE(TAG, "Alignment consumed the whole partition %u (aligned start %" PRIu32 " >= end %" PRIu64 ")",
                         partition_index, aligned_start, orig_end);
                return ESP_ERR_INVALID_SIZE;
            }
            sector_count = orig_end - (uint64_t) aligned_start;
            break;
        }
        default:
            ESP_LOGE(TAG, "Unknown align_policy %d", (int) extra_args->align_policy);
            return ESP_ERR_INVALID_ARG;
        }
    }

    // The exclusive end LBA (start + count) must also fit in the 32-bit MBR fields.
    // Checking start and count individually (above) is not enough - their sum can
    // still exceed UINT32_MAX. Compute it in 64-bit and reject if it overflows;
    // this also prevents the auto-placement cursor (lba_start + sector_count) from
    // wrapping in esp_mbr_generate.
    if ((uint64_t) aligned_start + sector_count > (uint64_t) UINT32_MAX) {
        ESP_LOGE(TAG, "Partition end (sector %" PRIu64 ") exceeds 32-bit limit of MBR",
                 (uint64_t) aligned_start + sector_count);
        return ESP_ERR_NOT_SUPPORTED;
    }

    partition->lba_start = aligned_start;
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
    // (initializer follows the struct's member order)
    esp_mbr_generate_extra_args_t args = {
        .total_size = 0, // Default: no "fits within disk" check
        .sector_size = part_list->sector_size != ESP_EXT_PART_SECTOR_SIZE_UNKNOWN ? part_list->sector_size : ESP_EXT_PART_SECTOR_SIZE_512B, // Default sector size
        .alignment = ESP_EXT_PART_ALIGN_AUTO, // Resolved to the default (1 MiB) below unless overridden
        .align_policy = ESP_EXT_PART_ALIGN_POLICY_KEEP_SIZE, // Default: keep the requested size (current behavior)
        .keep_signature = false, // Default is to generate a new disk signature
    };

    // Load extra arguments if provided
    if (extra_args) {
        if (extra_args->sector_size != ESP_EXT_PART_SECTOR_SIZE_UNKNOWN) {
            args.sector_size = extra_args->sector_size;
        }
        if (extra_args->alignment != ESP_EXT_PART_ALIGN_AUTO) {
            // Honor an explicit alignment, including ESP_EXT_PART_ALIGN_NONE
            args.alignment = extra_args->alignment;
        }
        args.keep_signature = extra_args->keep_signature;
        if (extra_args->esp_mbr_generate_custom_supported_partition_types) {
            args.esp_mbr_generate_custom_supported_partition_types = extra_args->esp_mbr_generate_custom_supported_partition_types;
        }
        args.align_policy = extra_args->align_policy;
        args.total_size = extra_args->total_size;
    }

    // Resolve ESP_EXT_PART_ALIGN_AUTO to the library default alignment (1 MiB)
    if (args.alignment == ESP_EXT_PART_ALIGN_AUTO) {
        args.alignment = ESP_EXT_PART_ALIGN_1MiB;
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

    // Total disk size in sectors (used both for auto-fill/FILL and the bounds check).
    // Use FLOOR division here: this is a device capacity, so a trailing partial sector
    // (when total_size is not a whole multiple of sector_size) is not usable and must
    // not be counted - otherwise a FILL/bounds check could place a partition up to one
    // sector past the real end of the disk.
    uint64_t total_sectors = 0;
    if (args.total_size != 0 && args.sector_size != ESP_EXT_PART_SECTOR_SIZE_UNKNOWN) {
        total_sectors = args.total_size / (uint64_t) args.sector_size;
    }

    esp_ext_part_list_item_t *it = NULL;
    int i = 0;
    // Running cursor for automatic placement (in sectors). Starts at 1 so the first
    // auto-placed partition begins after the MBR sector (sector 0) and, once aligned,
    // lands on the first aligned LBA (e.g. sector 2048 for 1 MiB / 512 B).
    uint32_t next_free_lba = 1;
    SLIST_FOREACH(it, &part_list->head, next) {
        if (i >= MBR_MAX_PARTITION_COUNT) {
            ESP_LOGW(TAG, "More than %d partitions in the list, only the first %d will be added to the MBR", MBR_MAX_PARTITION_COUNT, MBR_MAX_PARTITION_COUNT);
            break; // MBR can only hold 4 partitions
        }

        // An empty (ESP_EXT_PART_TYPE_NONE) item cannot be written as a partition entry:
        // it would leave a zeroed slot in the middle of the table, which esp_mbr_parse
        // stops at (silently dropping later partitions). Reject it.
        if (it->info.type == ESP_EXT_PART_TYPE_NONE) {
            ESP_LOGE(TAG, "Empty partition (ESP_EXT_PART_TYPE_NONE) in list would create a gap in the MBR partition table");
            return ESP_ERR_INVALID_ARG;
        }

        // FILL without AUTO_ADDRESS has no effect (FILL is only resolved inside the
        // AUTO_ADDRESS block). Reject the case where size is also 0; that combination
        // would silently produce a zero-sector entry.
        if ((it->info.flags & ESP_EXT_PART_FLAG_FILL) && !(it->info.flags & ESP_EXT_PART_FLAG_AUTO_ADDRESS)) {
            if (it->info.size == 0) {
                ESP_LOGE(TAG, "Partition %d has FILL flag without AUTO_ADDRESS and size 0; this would write a zero-size partition", i);
                return ESP_ERR_INVALID_ARG;
            }
            ESP_LOGW(TAG, "Partition %d has FILL flag without AUTO_ADDRESS; FILL has no effect", i);
        }

        // Work on a shallow copy so the caller's list items are never mutated.
        esp_ext_part_list_item_t local = *it;

        if (it->info.flags & ESP_EXT_PART_FLAG_AUTO_ADDRESS) {
            // Compute an aligned start placed after the previous entry.
            uint32_t start_lba = esp_mbr_lba_align(next_free_lba, args.sector_size, args.alignment);

            uint64_t size_sectors;
            if (it->info.size == 0) {
                if (!(it->info.flags & ESP_EXT_PART_FLAG_FILL)) {
                    ESP_LOGE(TAG, "Partition %d has AUTO_ADDRESS and size 0 but no FILL flag", i);
                    return ESP_ERR_INVALID_ARG;
                }
                if (total_sectors == 0 || (uint64_t) start_lba >= total_sectors) {
                    ESP_LOGE(TAG, "Partition %d FILL cannot size to disk end (total sectors %" PRIu64 ", start %" PRIu32 ")",
                             i, total_sectors, start_lba);
                    return ESP_ERR_INVALID_SIZE;
                }
                size_sectors = total_sectors - (uint64_t) start_lba;
            } else {
                size_sectors = esp_ext_part_bytes_to_sector_count(it->info.size, args.sector_size);
            }

            // Feed a concrete, already-aligned address/size to esp_mbr_partition_set.
            local.info.address = esp_ext_part_sector_count_to_bytes(start_lba, args.sector_size);
            local.info.size = esp_ext_part_sector_count_to_bytes(size_sectors, args.sector_size);
            local.info.flags &= ~(esp_ext_part_flags_t) ESP_EXT_PART_FLAG_AUTO_ADDRESS; // Now concrete
        }

        err = esp_mbr_partition_set(mbr, i, &local, &args);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set partition %d: %s", i, esp_err_to_name(err));
            return err; // Error setting partition
        }

        // Advance the cursor from the entry actually written (post-alignment).
        next_free_lba = mbr->partition_table[i].lba_start + mbr->partition_table[i].sector_count;
        i += 1;
    }
    int partition_count = i; // Number of partition entries actually written

    // Validate the generated layout (using the final post-alignment LBA values).
    for (int a = 0; a < partition_count; a++) {
        mbr_partition_t *pa = &mbr->partition_table[a];
        if (pa->type == 0x00) {
            continue; // Empty entry, nothing to validate
        }
        uint64_t a_start = pa->lba_start;
        uint64_t a_end = a_start + pa->sector_count; // exclusive

        // "Fits within disk" check (only when a total size was provided/auto-filled).
        if (total_sectors != 0 && a_end > total_sectors) {
            ESP_LOGE(TAG, "Partition %d (sectors %" PRIu64 "..%" PRIu64 ") runs past the disk (%" PRIu64 " sectors)",
                     a, a_start, a_end, total_sectors);
            return ESP_ERR_INVALID_SIZE;
        }

        // Overlap check against previously placed partitions.
        for (int b = 0; b < a; b++) {
            mbr_partition_t *pb = &mbr->partition_table[b];
            if (pb->type == 0x00) {
                continue;
            }
            uint64_t b_start = pb->lba_start;
            uint64_t b_end = b_start + pb->sector_count; // exclusive
            if (a_start < b_end && b_start < a_end) {
                ESP_LOGE(TAG, "Partition %d (sectors %" PRIu64 "..%" PRIu64 ") overlaps partition %d (sectors %" PRIu64 "..%" PRIu64 ")",
                         a, a_start, a_end, b, b_start, b_end);
                return ESP_ERR_INVALID_STATE;
            }
        }
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
