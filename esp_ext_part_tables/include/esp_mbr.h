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

#define ESP_MBR_SIZE 512
#define ESP_MBR_SIGNATURE 0xAA55
#define ESP_MBR_COPY_PROTECTED 0x5A5A
#define ESP_MBR_PARTITION_TABLE_OFFSET 0x1BE
#define ESP_MBR_PARTITION_STATUS_ACTIVE 0x80
#define ESP_MBR_MAX_PARTITION_COUNT 4
#define ESP_MBR_PARTITION_TYPE_GPT_PROTECTIVE 0xEE

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
} esp_mbr_partition_t;
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
    esp_mbr_partition_t partition_table[ESP_MBR_MAX_PARTITION_COUNT];
    uint16_t boot_signature;
} esp_mbr_t;
#pragma pack(pop)

typedef struct {
    esp_ext_part_sector_size_t sector_size; // Sector size hint, pulled from a storage device driver query
    bool (*esp_mbr_parse_custom_supported_partition_types)(uint8_t, uint8_t *); // Custom function for parsing supported MBR partition types, optional
    esp_ext_part_match_t match; // Optional filter: if match.fn is non-NULL, only recognized partitions for which match.fn(info, match.ctx) returns true are inserted (the rest are dropped and the list is marked ESP_EXT_PART_LIST_FLAG_LOSSY). A zero-initialized match (fn == NULL) inserts all recognized partitions.
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
 * By default every recognized partition is inserted into the list, regardless of
 * whether ESP-IDF can mount it. To filter at parse time, set `extra_args->match` to
 * a predicate: only recognized partitions for which it returns true are inserted
 * (for example `esp_ext_part_match_mountable`). After parsing, a subset can also be
 * iterated with `esp_ext_part_list_next_matching`. When any partition is skipped (an
 * unknown/extended type, or one rejected by `match`), the list is marked
 * `ESP_EXT_PART_LIST_FLAG_LOSSY`.
 *
 * @note A GPT disk carries a protective MBR in its first sector, so this function
 *       succeeds on one and returns a single partition of type
 *       `ESP_EXT_PART_TYPE_GPT_PROTECTIVE_MBR` spanning the device. Those are not the
 *       real partitions; this component cannot read a GPT table. Check for that type,
 *       or probe the device with `esp_ext_part_probe`, if GPT media are possible.
 *
 * @note This function is not thread-safe.
 *
 * @param[in]  mbr_buf    Pointer to a buffer containing the raw MBR data (must be at least `ESP_MBR_SIZE` bytes and start of the MBR must align with start of the buffer).
 * @param[out] part_list  Pointer to the partition list structure to be filled with parsed entries. Must be empty (zero-initialized, or emptied with `esp_ext_part_list_deinit`), because the parsed table fully defines the list.
 * @param[in]  extra_args Optional extra arguments for parsing (can be NULL for defaults).
 *
 * @return
 *     - ESP_OK:                Parsing was successful.
 *     - ESP_ERR_INVALID_ARG:   Invalid arguments were provided.
 *     - ESP_ERR_INVALID_STATE: `part_list` already holds partitions.
 *     - ESP_ERR_NOT_FOUND:     MBR signature not found or invalid MBR.
 *     - ESP_ERR_NO_MEM:        Memory allocation failed during parsing.
 *     - Other error codes from `esp_ext_part_list_insert`.
 */
esp_err_t esp_mbr_parse(const void *mbr_buf,
                        esp_ext_part_list_t *part_list,
                        const esp_mbr_parse_extra_args_t *extra_args);

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
 * @note The partition table is fully rewritten from `part_list`: every one of the
 *       `ESP_MBR_MAX_PARTITION_COUNT` entries is either built from a list item or
 *       zeroed, so entries left over from a previously loaded MBR never survive. The
 *       bootstrap code area is preserved, which makes read-modify-write of an existing
 *       MBR (typically together with `keep_signature`) safe. The copy-protection
 *       marker follows `ESP_EXT_PART_LIST_FLAG_NONE`/`ESP_EXT_PART_LIST_FLAG_READ_ONLY`
 *       in both directions.
 *
 * @note This function is not thread-safe.
 *
 * @param[out] mbr         Pointer to the MBR structure to be filled (must already be allocated and be at least `ESP_MBR_SIZE` bytes). May contain a previously loaded MBR.
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
esp_err_t esp_mbr_generate(esp_mbr_t *mbr,
                           const esp_ext_part_list_t *part_list,
                           const esp_mbr_generate_extra_args_t *extra_args);

/**
 * @brief Sets a partition entry in the MBR (Master Boot Record).
 *
 * This function updates the specified partition entry in the provided MBR structure
 * with the information from the given partition list item. Additional arguments for
 * partition generation must be supplied via the extra_args parameter.
 *
 * When alignment moves the partition start, `extra_args->align_policy` decides how
 * the size is treated (keep it, reject, or shrink to preserve the end; see
 * `esp_ext_part_align_policy_t`). This low-level function resolves no defaults:
 * `extra_args->sector_size` must be set to a concrete value (passing
 * `ESP_EXT_PART_SECTOR_SIZE_UNKNOWN` returns `ESP_ERR_INVALID_ARG` rather than
 * assuming 512 B), and `ESP_EXT_PART_ALIGN_AUTO` is not resolved to the 1 MiB
 * default either - callers that want those defaults should go through
 * `esp_mbr_generate`. It also does NOT perform overlap or disk-bounds validation
 * (that is done by `esp_mbr_generate`).
 *
 * Clearing an entry (`item->info.type == ESP_EXT_PART_TYPE_NONE`) needs no sector
 * size and is always accepted.
 *
 * The target entry is written in full: every field is either derived from `item` or
 * zeroed, so no value from a previously present partition survives. On error the
 * entry is left untouched.
 *
 * @note This function is not thread-safe.
 *
 * @warning If the partition entry is empty (i.e., `item->info.type` is `ESP_EXT_PART_TYPE_NONE`), it will be cleared in the MBR.
 *          If there is an empty gap between partition entries, partition entries after the gap will most likely be ignored when the MBR is parsed (MBR does not allow gaps in the partition table).
 *          To avoid this, you can use `esp_mbr_remove_gaps_between_partition_entries()` function to remove gaps in the MBR partition table.
 *
 * @param[in,out] mbr               Pointer to the MBR structure to be updated.
 * @param[in]     partition_index   Index of the partition entry to set (0-3).
 * @param[in]     item              Pointer to the partition list item structure containing partition information.
 * @param[in]     extra_args        Extra arguments for partition entry setting (required, and its `sector_size` must be set unless the entry is being cleared).
 *
 * @return
 *     - ESP_OK:                Success.
 *     - ESP_ERR_INVALID_ARG:   Invalid arguments were provided, `extra_args->sector_size` is `ESP_EXT_PART_SECTOR_SIZE_UNKNOWN`, or the start was not aligned while `align_policy` is `ESP_EXT_PART_ALIGN_POLICY_REJECT`.
 *     - ESP_ERR_INVALID_STATE: Error filling partition entry.
 *     - ESP_ERR_INVALID_SIZE:  Alignment consumed the whole partition (PRESERVE_END policy).
 *     - ESP_ERR_NOT_SUPPORTED: Partition address or size (sector count) exceeds 32-bit limit of MBR.
 */
esp_err_t esp_mbr_partition_set(esp_mbr_t *mbr, uint8_t partition_index, const esp_ext_part_list_item_t *item, const esp_mbr_generate_extra_args_t *extra_args);

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
esp_err_t esp_mbr_remove_gaps_between_partition_entries(esp_mbr_t *mbr);

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
/**
 * @brief Read the MBR from a block device and parse it into a partition list.
 *
 * Reads the first sector of the device and hands it to `esp_mbr_parse`.
 *
 * The first sector of a partitioned medium is always an MBR, so this is a valid first
 * step even on a device whose format is unknown. On a GPT disk it succeeds and yields
 * a single partition of type `ESP_EXT_PART_TYPE_GPT_PROTECTIVE_MBR` covering the
 * device - that is the protective MBR, not the real GPT table, which this component
 * cannot read. Use `esp_ext_part_probe` first if you need to know the format up front.
 *
 * @note This function is not thread-safe.
 *
 * @param[in]  handle     Block device handle to read from.
 * @param[out] part_list  Partition list to populate. Must be empty (zero-initialized, or emptied with `esp_ext_part_list_deinit`).
 * @param[in]  extra_args Optional extra arguments for parsing (can be NULL for defaults).
 *
 * @return
 *     - ESP_OK: Partition list was successfully loaded.
 *     - ESP_ERR_INVALID_ARG: `handle` or `part_list` is NULL.
 *     - ESP_ERR_INVALID_STATE: `part_list` already holds partitions.
 *     - ESP_ERR_NOT_FOUND: MBR signature not found.
 *     - ESP_ERR_NO_MEM: Memory allocation failed.
 *     - propagated errors from BDL operations.
 */
esp_err_t esp_mbr_bdl_read(esp_blockdev_handle_t handle,
                           esp_ext_part_list_t *part_list,
                           const esp_mbr_parse_extra_args_t *extra_args);

/**
 * @brief Generate an MBR from a partition list and write it to a block device.
 *
 * Generates the MBR with `esp_mbr_generate` and writes it to the first sector.
 *
 * @note The caller's `extra_args` is never modified. When its `total_size` is 0, a
 *       copy is made with `total_size` filled in from the block device geometry, so
 *       the "fits within disk" check is performed by default.
 *
 * @warning The MBR is generated into a zeroed buffer, so the bootstrap code of any MBR
 *          already on the device is not preserved by this function. Use `esp_mbr_parse`
 *          plus `esp_mbr_generate` on a buffer you read yourself if you need to keep it.
 *
 * @note This function is not thread-safe.
 *
 * @param[in] handle     Block device handle to write to.
 * @param[in] part_list  Partition list to generate the MBR from.
 * @param[in] extra_args Optional extra arguments for generation (can be NULL for defaults).
 *
 * @return
 *     - ESP_OK: Partition table was successfully written.
 *     - ESP_ERR_INVALID_ARG: `handle` or `part_list` is NULL.
 *     - ESP_ERR_NO_MEM: Memory allocation failed.
 *     - propagated errors from BDL operations or from `esp_mbr_generate`.
 */
esp_err_t esp_mbr_bdl_write(esp_blockdev_handle_t handle,
                            const esp_ext_part_list_t *part_list,
                            const esp_mbr_generate_extra_args_t *extra_args);
#endif // (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))

#ifdef __cplusplus
}
#endif
