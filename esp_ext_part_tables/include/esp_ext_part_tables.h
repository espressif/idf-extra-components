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
#include "esp_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
#include "esp_blockdev.h"
#endif // (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))

#if __has_include(<bsd/sys/queue.h>)
#include <bsd/sys/queue.h>
#else
#include "sys/queue.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_EXT_PART_SECTOR_SIZE_UNKNOWN = 0, // Unknown sector size
    ESP_EXT_PART_SECTOR_SIZE_512B = 512, // 512 B sector size (SD, eMMC, USB flash, legacy or emulated mode HDD/SSD)
    ESP_EXT_PART_SECTOR_SIZE_2KiB = 2048, // 2 KiB sector size (optical disks)
    ESP_EXT_PART_SECTOR_SIZE_4KiB = 4096, // 4 kiB sector size (modern HDD/SSD)
} esp_ext_part_sector_size_t;

typedef enum {
    ESP_EXT_PART_ALIGN_AUTO = 0, // Use the library default alignment (1 MiB). This is what a zero-initialized extra_args selects.
    ESP_EXT_PART_ALIGN_4KiB = 4096, // 4 KiB alignment
    ESP_EXT_PART_ALIGN_1MiB = (1024 * 1024), // 1 MiB alignment
    ESP_EXT_PART_ALIGN_NONE = -1, // Explicitly perform no alignment (leave LBAs untouched). A representable-in-int sentinel (not a real alignment value).
} esp_ext_part_align_t;

typedef enum {
    /*!< Default: align the partition start up, keep the requested size as the length from the aligned start (matches fdisk/parted behavior). */
    ESP_EXT_PART_ALIGN_POLICY_KEEP_SIZE = 0,
    /*!< Return an error if a partition start was not already aligned (do not silently relocate it). */
    ESP_EXT_PART_ALIGN_POLICY_REJECT,
    /*!< Align the partition start up, then shrink the size so the end stays at the originally requested address + size. Errors if alignment consumes the whole partition. */
    ESP_EXT_PART_ALIGN_POLICY_PRESERVE_END,
} esp_ext_part_align_policy_t;

typedef enum __attribute__((packed))
{
    ESP_EXT_PART_TYPE_NONE = 0x00,
    ESP_EXT_PART_TYPE_FAT12,
    ESP_EXT_PART_TYPE_FAT16, /*!< FAT16 with LBA addressing */
    ESP_EXT_PART_TYPE_FAT32, /*!< FAT32 with LBA addressing */
    ESP_EXT_PART_TYPE_LITTLEFS, /*!< Possibly LittleFS (MBR CHS field => LittleFS block size hack) */
    ESP_EXT_PART_TYPE_RAW_DATA, /*!< Non-filesystem/custom data partition (e.g. raw data, custom format, etc.) */
// Note: The following types are not supported, but we can return a type for them
    ESP_EXT_PART_TYPE_LINUX_ANY, /*!< Linux partition (any type) */
    ESP_EXT_PART_TYPE_EXFAT_OR_NTFS, /*!< Not supported, but we can return a type for it */
    ESP_EXT_PART_TYPE_GPT_PROTECTIVE_MBR, /*!< Not supported, but we can return a type for it */
} esp_ext_part_type_known_t;

typedef enum {
    ESP_EXT_PART_FLAG_NONE = 0,
    ESP_EXT_PART_FLAG_ACTIVE = 1 << 0,  /*!< Active / bootable partition */
    ESP_EXT_PART_FLAG_EXTRA = 1 << 1, /*!< Additional information stored in `extra` field (e.g. LittleFS block size stored in CHS hack) */
    ESP_EXT_PART_FLAG_AUTO_ADDRESS = 1 << 2, /*!< During MBR generation, let the library compute the start address (placed after the previous partition, aligned). `info.address` is ignored. */
    ESP_EXT_PART_FLAG_FILL = 1 << 3, /*!< During MBR generation, together with ESP_EXT_PART_FLAG_AUTO_ADDRESS and `info.size == 0`, size the partition to fill from its computed start to the end of the disk (requires a known total size). */
} esp_ext_part_flags_t;

typedef enum {
    ESP_EXT_PART_LIST_FLAG_NONE = 0,
    ESP_EXT_PART_LIST_FLAG_READ_ONLY = 1 << 0, /*!< Read-only partition list */
    ESP_EXT_PART_LIST_FLAG_LOSSY = 1 << 1, /*!< Set by the parser when one or more source partitions were skipped (an unknown/extended type, or one rejected by the parse `match` predicate), so a regenerated table would NOT be functionally equivalent to the source. Unset = every recognized partition was captured. */
} esp_ext_part_list_flags_t;

typedef enum {
    ESP_EXT_PART_LIST_SIGNATURE_MBR, /*!< MBR signature type */
    ESP_EXT_PART_LIST_SIGNATURE_GPT, /*!< GPT. Reported by `esp_ext_part_probe` only; this component cannot parse or generate GPT, and the signature accessors reject this type. */
} esp_ext_part_signature_type_t;

typedef struct {
    uint32_t data[1];
    esp_ext_part_signature_type_t type;
} esp_ext_part_list_signature_t;

typedef struct {
    uint64_t address; /*!< Start address in bytes */
    uint64_t size; /*!< Size in bytes */
    uint64_t extra; /*!< Extra information (e.g. LittleFS block size stored in CHS hack, etc.) */
    char *label;
    esp_ext_part_flags_t flags; /*!< Flags for the partition */
    uint8_t type; /*!< Known partition type for this component (usually a part of `esp_ext_part_type_known_t`) */
} esp_ext_part_t;

typedef struct esp_ext_part_list_item_ {
    esp_ext_part_t info;
    SLIST_ENTRY(esp_ext_part_list_item_) next;
} esp_ext_part_list_item_t;

typedef struct {
    esp_ext_part_list_signature_t signature; /*!< Disk signature or identifier */
    SLIST_HEAD(esp_ext_part_list_head_, esp_ext_part_list_item_) head; /*!< Head of the partition list */
    esp_ext_part_list_flags_t flags; /*!< Flags for the partition list */
    esp_ext_part_sector_size_t sector_size; /*!< Sector size (storage medium property). `esp_mbr_parse` sets this from the parsed medium. For a freshly built list you may set it directly (e.g. `list.sector_size = ESP_EXT_PART_SECTOR_SIZE_4KiB;`); `esp_mbr_generate` uses it as the default sector size, unless overridden by `esp_mbr_generate_extra_args_t::sector_size`. Left `ESP_EXT_PART_SECTOR_SIZE_UNKNOWN` (0) it defaults to 512 B. */
} esp_ext_part_list_t;

/**
 * @brief Convert bytes to sector count based on the sector size.
 *
 * This function performs a ceiling division to ensure that any remaining bytes
 * that do not fill a complete sector are counted as an additional sector.
 *
 * @param total_bytes Total number of bytes.
 * @param sector_size Size of a single sector.
 *
 * @return Number of sectors or 0 if the sector size is unknown to avoid a division by zero.
 */
uint64_t esp_ext_part_bytes_to_sector_count(uint64_t total_bytes, esp_ext_part_sector_size_t sector_size);

/**
 * @brief Convert sector count to bytes based on the sector size.
 *
 * @param sector_count Number of sectors.
 * @param sector_size Size of a single sector.
 *
 * @return Total size in bytes.
 */
uint64_t esp_ext_part_sector_count_to_bytes(uint64_t sector_count, esp_ext_part_sector_size_t sector_size);

/**
 * @brief Deinitialize an external partition list structure and free all resources.
 *
 * This function releases all the memory and resources associated with the partition list referenced by 'part_list' parameter.
 *
 * @note This function is not thread-safe.
 *
 * @param[in] part_list Pointer to the partition list structure to deinitialize.
 *
 * @return
 *     - ESP_OK: Deinitialization was successful.
 *     - ESP_ERR_INVALID_ARG: `part_list` is NULL.
 */
esp_err_t esp_ext_part_list_deinit(esp_ext_part_list_t *part_list);

/**
 * @brief Insert a partition item into an external partition list.
 *
 * This function inserts a copy of the given partition item into the partition list.
 *
 * @note This function is not thread-safe.
 *
 * @param[in] part_list Pointer to the partition list structure.
 * @param[in] item      Pointer to the partition item to insert (will be copied).
 *
 * @return
 *     - ESP_OK: Insertion was successful.
 *     - ESP_ERR_INVALID_ARG: `part_list` or `item` is NULL.
 *     - ESP_ERR_NO_MEM: Memory allocation failed.
 */
esp_err_t esp_ext_part_list_insert(esp_ext_part_list_t *part_list, const esp_ext_part_list_item_t *item);

/**
 * @brief Deep copy an external partition list.
 *
 * This function creates a deep copy of the source partition list into the destination partition list.
 * It allocates memory for the destination list and copies all items, including their labels.
 *
 * Use this instead of assigning or `memcpy`ing an `esp_ext_part_list_t`: the list holds
 * its items in an intrusive linked list and owns each item's `label` allocation, so a
 * plain struct copy would produce two lists sharing the same items, and deinitializing
 * both would free them twice.
 *
 * @note This function is not thread-safe.
 *
 * @param[out] dst Pointer to the destination partition list structure (must be allocated before and be empty, i.e. zero-initialized or emptied with `esp_ext_part_list_deinit`).
 * @param[in] src Pointer to the source partition list structure to copy from.
 *
 * @return
 *     - ESP_OK: Deep copy was successful.
 *     - ESP_ERR_INVALID_ARG: `dst` or `src` is NULL.
 *     - ESP_ERR_INVALID_STATE: `dst` already holds partitions.
 *     - ESP_ERR_NO_MEM: Memory allocation failed.
 */
esp_err_t esp_ext_part_list_deep_copy(esp_ext_part_list_t *dst, const esp_ext_part_list_t *src);

/**
 * @brief Get the head (first item) of an external partition list.
 *
 * @param[in] part_list Pointer to the partition list structure.
 *
 * @return Pointer to the first partition list item, or NULL if the list is empty or uninitialized.
 */
esp_ext_part_list_item_t *esp_ext_part_list_item_head(esp_ext_part_list_t *part_list);

/**
 * @brief Get the next item in an external partition list.
 *
 * @param[in] item Pointer to the current partition list item.
 *
 * @return Pointer to the next partition list item, or NULL if there are no more items.
 */
esp_ext_part_list_item_t *esp_ext_part_list_item_next(esp_ext_part_list_item_t *item);

/**
 * @brief Caller-supplied predicate deciding whether a partition matches.
 *
 * @param[in] info Partition info (type, address, size, flags, extra, label).
 * @param[in] ctx  Opaque caller context passed through unchanged (may be NULL).
 *
 * @return true to select this partition, false to skip it.
 */
typedef bool (*esp_ext_part_match_fn)(const esp_ext_part_t *info, void *ctx);

/**
 * @brief A predicate together with its opaque context.
 *
 * Bundles `fn` and the `ctx` passed to it, so a matcher can be stored and passed
 * around as a single value. A zero-initialized matcher (`fn == NULL`) matches
 * nothing.
 */
typedef struct {
    esp_ext_part_match_fn fn; /*!< Predicate; NULL means "match nothing". */
    void *ctx;                /*!< Opaque context passed to `fn` (may be NULL). */
} esp_ext_part_match_t;

/**
 * @brief Iterate partition list items matching a caller-supplied predicate.
 *
 * The predicate can branch on any partition field and on caller/build state - for
 * example "mountable in THIS build", which depends on which filesystem drivers are
 * linked and therefore cannot be decided by the library itself. Mirrors
 * `esp_ext_part_list_item_head` / `esp_ext_part_list_item_next`, but skips items the
 * predicate rejects. Pass NULL as `from` to search from the head, or a previously
 * returned item to continue (e.g. to find the N-th match).
 *
 * @param[in] from    Current item; pass NULL to start from the head of `list`.
 * @param[in] list    Partition list to iterate (used only when `from` is NULL).
 * @param[in] matcher Predicate + context; if NULL, or its `fn` is NULL, nothing matches.
 *
 * @return Pointer to the next matching item, or NULL if there are no more.
 */
esp_ext_part_list_item_t *esp_ext_part_list_next_matching(esp_ext_part_list_item_t *from, const esp_ext_part_list_t *list, const esp_ext_part_match_t *matcher);

/**
 * @brief Get a stock matcher selecting partitions ESP-IDF can mount.
 *
 * Returns a ready-to-use `esp_ext_part_match_t` for `esp_ext_part_list_next_matching`
 * (or `esp_mbr_parse_extra_args_t::match`) when you just want "the partitions this
 * build can mount" without writing your own predicate.
 *
 * - FAT12/16/32 are always considered mountable (FatFs is part of ESP-IDF).
 * - LittleFS is considered mountable only when the LittleFS component is available
 *   to this library at compile time - detected via `__has_include("esp_littlefs.h")`,
 *   or forced by defining `ESP_EXT_PART_HAS_LITTLEFS`. If neither applies, LittleFS
 *   is reported as not mountable (a safe under-report rather than a false claim).
 * - All other types (raw data, exFAT/NTFS, Linux, GPT-protective, none) are not
 *   mountable.
 *
 * @return A matcher whose `fn` reports whether a partition is mountable in this build.
 */
esp_ext_part_match_t esp_ext_part_match_mountable(void);

/**
 * @brief Get the signature of an external partition list.
 *
 * Retrieves the disk signature or identifier from the partition list. The output
 * carries both the raw signature data and its `type`, so the caller does not have
 * to know the width of the signature in advance. For
 * `ESP_EXT_PART_LIST_SIGNATURE_MBR` the 32-bit disk signature is in `data[0]`.
 *
 * @param[in]  part_list Pointer to the partition list structure.
 * @param[out] signature Pointer to the signature structure to fill.
 *
 * @return
 *     - ESP_OK: Signature retrieval was successful.
 *     - ESP_ERR_INVALID_ARG: `part_list` or `signature` is NULL.
 *     - ESP_ERR_NOT_SUPPORTED: Unsupported signature type stored in the list.
 */
esp_err_t esp_ext_part_list_signature_get(const esp_ext_part_list_t *part_list, esp_ext_part_list_signature_t *signature);

/**
 * @brief Set the signature of an external partition list.
 *
 * Sets the disk signature or identifier for the partition list. The signature type
 * is taken from `signature->type`; for `ESP_EXT_PART_LIST_SIGNATURE_MBR` the 32-bit
 * disk signature is read from `signature->data[0]`.
 *
 * @param[in] part_list Pointer to the partition list structure.
 * @param[in] signature Pointer to the signature structure to set.
 *
 * @return
 *     - ESP_OK: Signature was successfully set.
 *     - ESP_ERR_INVALID_ARG: `part_list` or `signature` is NULL.
 *     - ESP_ERR_NOT_SUPPORTED: Unsupported signature type.
 */
esp_err_t esp_ext_part_list_signature_set(esp_ext_part_list_t *part_list, const esp_ext_part_list_signature_t *signature);

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
/**
 * @brief Detect which partition table format a block device carries.
 *
 * Reads the first sector and reports the partition table format found there, so a
 * caller that does not know the medium in advance can pick the matching parser
 * instead of guessing.
 *
 * Detection relies on the first sector always being an MBR: a GPT disk carries a
 * protective MBR there, whose single partition entry has type `0xEE`. A disk with a
 * boot signature but no protective entry is reported as MBR.
 *
 * @note Only formats this component can parse are reported; see
 *       `esp_ext_part_signature_type_t`. A GPT disk is detected but cannot be parsed
 *       by this component, so `esp_mbr_bdl_read` on such a device returns only the
 *       protective entry (`ESP_EXT_PART_TYPE_GPT_PROTECTIVE_MBR`).
 *
 * @note This function is not thread-safe.
 *
 * @param[in]  handle   Block device handle to probe.
 * @param[out] out_type Detected partition table format.
 *
 * @return
 *     - ESP_OK: A known partition table format was detected.
 *     - ESP_ERR_INVALID_ARG: `handle` or `out_type` is NULL.
 *     - ESP_ERR_NOT_FOUND: No partition table was recognized (no MBR boot signature).
 *     - ESP_ERR_NO_MEM: Memory allocation failed.
 *     - propagated errors from BDL operations.
 */
esp_err_t esp_ext_part_probe(esp_blockdev_handle_t handle, esp_ext_part_signature_type_t *out_type);
#endif // (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))

#ifdef __cplusplus
}
#endif
