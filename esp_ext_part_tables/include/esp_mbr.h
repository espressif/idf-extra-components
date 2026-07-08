/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_ext_part_tables.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MBR_SIZE 512
#define MBR_SIGNATURE 0xAA55
#define MBR_COPY_PROTECTED 0x5A5A
#define MBR_PARTITION_TABLE_OFFSET 0x1BE
#define MBR_PARTITION_STATUS_ACTIVE 0x80
#define MBR_MAX_PARTITION_COUNT 4

// MBR partition entry structure - https://en.wikipedia.org/wiki/Master_boot_record#Partition_table_entries
#pragma pack(push, 1)
typedef struct {
    uint8_t status;
    union {
        struct {
            uint8_t h_start;
            uint16_t cs_start;
        };
        uint8_t chs_start[3];
    };
    uint8_t type;
    union {
        struct {
            uint8_t h_end;
            uint16_t cs_end;
        };
        uint8_t chs_end[3];
    };
    uint32_t lba_start;
    uint32_t sector_count;
} mbr_partition_t;
#pragma pack(pop)

// MBR structure - https://en.wikipedia.org/wiki/Master_boot_record#Sector_layout
#pragma pack(push, 1)
typedef struct {
    union {
        uint8_t bootstrap_code_classical[446];
        struct {
            uint8_t bootstrap_code_modern_part1[218];
            uint16_t _reserved;
            uint8_t original_physical_drive;
            uint8_t seconds;
            uint8_t minutes;
            uint8_t hours;
            uint8_t bootstrap_code_modern_part2[216];
            uint32_t disk_signature;
            uint16_t copy_protected;
        };
    };
    mbr_partition_t partition_table[4];
    uint16_t boot_signature;
} mbr_t;
#pragma pack(pop)

typedef struct {
    esp_ext_part_sector_size_t sector_size; // Sector size hint, pulled from a storage device driver query
    bool (*esp_mbr_parse_custom_supported_partition_types)(uint8_t, uint8_t *); // Custom function for parsing supported MBR partition types, optional
} esp_mbr_parse_extra_args_t;

typedef struct {
    // Members are ordered to minimize struct padding (8-byte fields, then 4-byte
    // enums, then bools). Zero-initialization ({0}) still selects all defaults.
    uint64_t total_size; // Total device size in bytes for the "fits within disk" check; 0 disables the check. The BDL write helper auto-fills this from the device geometry when left 0.
    uint8_t (*esp_mbr_generate_custom_supported_partition_types)(uint8_t); // Custom function for generating supported MBR partition types, optional
    esp_ext_part_sector_size_t sector_size; // Sector size for correct LBA alignment. Overrides the list's `sector_size` for this call; 0 (UNKNOWN) falls back to the list's value, then to 512 B.
    esp_ext_part_align_t alignment; // Partition start alignment. 0 (ESP_EXT_PART_ALIGN_AUTO, the default) resolves to 1 MiB; ESP_EXT_PART_ALIGN_NONE leaves LBAs untouched; ESP_EXT_PART_ALIGN_4KiB / ESP_EXT_PART_ALIGN_1MiB request a specific alignment.
    esp_ext_part_align_policy_t align_policy; // Policy applied when alignment moves a partition start (default 0 = KEEP_SIZE)
    bool keep_signature; // If true, the disk signature will be preserved in the generated MBR and not overwritten with a random value
} esp_mbr_generate_extra_args_t;

/**
 * @brief Parses a Master Boot Record (MBR) buffer and extracts partition information.
 *
 * This function reads the provided MBR buffer, validates its signature, and populates
 * the given partition list structure with the partition entries found in the MBR.
 * Additional parsing options can be provided via the extra_args parameter.
 *
 * @note This function is not thread-safe.
 *
 * @param[in]  mbr_buf    Pointer to a buffer containing the raw MBR data (must be at least `MBR_SIZE` bytes and start of the MBR must align with start of the buffer).
 * @param[out] part_list  Pointer to the partition list structure to be filled with parsed entries.
 * @param[in]  extra_args Optional extra arguments for parsing (can be NULL for defaults).
 *
 * @return
 *     - ESP_OK:              Parsing was successful.
 *     - ESP_ERR_INVALID_ARG: Invalid arguments were provided.
 *     - ESP_ERR_NOT_FOUND:   MBR signature not found or invalid MBR.
 *     - ESP_ERR_NO_MEM:      Memory allocation failed during parsing.
 *     - Other error codes from `esp_ext_part_list_insert`.
 */
esp_err_t esp_mbr_parse(void *mbr_buf,
                        esp_ext_part_list_t *part_list,
                        esp_mbr_parse_extra_args_t *extra_args);

/**
 * @brief Generates a Master Boot Record (MBR) from a partition list.
 *
 * This function fills the provided MBR structure based on the given partition list.
 * It sets up the partition table, disk signature, and other MBR fields. Generation
 * options such as sector size, alignment, and signature preservation can be specified
 * via the extra_args parameter.
 *
 * After all partition entries are written, the generated layout is validated:
 * overlapping partitions are rejected, and if `extra_args->total_size` is
 * non-zero, partitions that run past the end of the disk are rejected.
 *
 * Alignment behavior:
 *   - `extra_args->alignment == ESP_EXT_PART_ALIGN_AUTO` (the default when a
 *     zero-initialized `extra_args` is used) resolves to a 1 MiB alignment.
 *   - `ESP_EXT_PART_ALIGN_NONE` leaves partition start LBAs untouched.
 *   - When alignment moves a partition start, `extra_args->align_policy` decides
 *     what happens to the size (see `esp_ext_part_align_policy_t`).
 *
 * Automatic placement:
 *   - A partition item with `ESP_EXT_PART_FLAG_AUTO_ADDRESS` has its start address
 *     computed by this function (placed after the previous entry, aligned); its
 *     `info.address` is ignored. The first such partition is placed at the first
 *     aligned LBA (after the MBR sector).
 *   - With `ESP_EXT_PART_FLAG_FILL` and `info.size == 0`, the partition is sized to
 *     fill from its computed start to the end of the disk; this requires
 *     `extra_args->total_size` (or, via `esp_ext_part_list_bdl_write`, the device
 *     geometry).
 *   - Auto-placement is honored only here (and through `esp_ext_part_list_bdl_write`);
 *     `esp_mbr_partition_set` does not support it. The caller's partition list is not
 *     modified.
 *
 * Empty list items:
 *   - A list item with `type == ESP_EXT_PART_TYPE_NONE` cannot be encoded as a
 *     partition entry without leaving a gap in the table (which truncates the parsed
 *     result and disturbs auto-placement). Such an item is rejected with
 *     `ESP_ERR_INVALID_ARG`.
 *
 * @note This function is not thread-safe.
 *
 * @param[out] mbr         Pointer to the blank MBR structure to be filled (must already be allocated and be at least `MBR_SIZE` bytes).
 * @param[in]  part_list   Pointer to the partition list structure containing partition entries to encode.
 * @param[in]  extra_args  Optional extra arguments for generation (can be NULL for defaults: 1 MiB alignment, KEEP_SIZE policy, no disk-bounds check).
 *
 * @return
 *     - ESP_OK:                Generation was successful.
 *     - ESP_ERR_INVALID_ARG:   Invalid arguments were provided, a partition start was not aligned while `align_policy` is `ESP_EXT_PART_ALIGN_POLICY_REJECT`, an AUTO_ADDRESS partition has size 0 without the FILL flag, or a list item has type `ESP_EXT_PART_TYPE_NONE`.
 *     - ESP_ERR_INVALID_STATE: Error filling a partition entry, or two partitions overlap.
 *     - ESP_ERR_INVALID_SIZE:  Alignment consumed a whole partition (PRESERVE_END policy), a partition runs past `total_size`, or a FILL partition cannot be sized (no/insufficient total size).
 *     - ESP_ERR_NOT_SUPPORTED: Partition address or size (sector count) exceeds 32-bit limit of MBR.
 *     - Other error codes from `esp_ext_part_list_signature_get` or `esp_mbr_partition_set`.
 */
esp_err_t esp_mbr_generate(mbr_t *mbr,
                           esp_ext_part_list_t *part_list,
                           esp_mbr_generate_extra_args_t *extra_args);

/**
 * @brief Sets a partition entry in the MBR (Master Boot Record).
 *
 * This function updates the specified partition entry in the provided MBR structure
 * with the information from the given partition list item. Additional arguments for
 * partition generation must be supplied via the extra_args parameter.
 *
 * When alignment moves the partition start, `extra_args->align_policy` decides how
 * the size is treated (keep it, reject, or shrink to preserve the end; see
 * `esp_ext_part_align_policy_t`). This low-level function does NOT resolve
 * `ESP_EXT_PART_ALIGN_AUTO` to a default alignment - callers that want the 1 MiB
 * default should either go through `esp_mbr_generate` or pass a concrete alignment
 * value. It also does NOT perform overlap or disk-bounds validation (that is done
 * by `esp_mbr_generate`).
 *
 * @note This function is not thread-safe.
 *
 * @warning If the partition entry is empty (i.e., `item->info.type` is `ESP_EXT_PART_TYPE_NONE`), it will be cleared in the MBR.
 *          If there is an empty gap between partition entries, partition entries after the gap will most likely be ignored when the MBR is parsed (MBR does not allow gaps in the partition table).
 *          To avoid this, you can use `esp_mbr_remove_gaps_between_partiton_entries()` function to remove gaps in the MBR partition table.
 *
 * @param[in,out] mbr               Pointer to the MBR structure to be updated.
 * @param[in]     partition_index   Index of the partition entry to set (0-3).
 * @param[in]     item              Pointer to the partition list item structure containing partition information.
 * @param[in]     extra_args        Extra arguments for partition entry setting (required).
 *
 * @return
 *     - ESP_OK:                Success.
 *     - ESP_ERR_INVALID_ARG:   Invalid arguments were provided, or the start was not aligned while `align_policy` is `ESP_EXT_PART_ALIGN_POLICY_REJECT`.
 *     - ESP_ERR_INVALID_STATE: Error filling partition entry.
 *     - ESP_ERR_INVALID_SIZE:  Alignment consumed the whole partition (PRESERVE_END policy).
 *     - ESP_ERR_NOT_SUPPORTED: Partition address or size (sector count) exceeds 32-bit limit of MBR.
 */
esp_err_t esp_mbr_partition_set(mbr_t *mbr, uint8_t partition_index, esp_ext_part_list_item_t *item, esp_mbr_generate_extra_args_t *extra_args);

/**
 * @brief Removes gaps in the MBR partition table by shifting partitions.
 *
 * @note This function is not thread-safe.
 *
 * @param[in,out] mbr Pointer to the MBR structure to be updated.
 * @return
 *     - ESP_OK: Success.
 *     - ESP_ERR_INVALID_ARG: Invalid pointer to MBR structure.
 */
esp_err_t esp_mbr_remove_gaps_between_partiton_entries(mbr_t *mbr);

#ifdef __cplusplus
}
#endif
