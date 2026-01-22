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
    ESP_EXT_PART_ALIGN_NONE = 0, // No alignment
    ESP_EXT_PART_ALIGN_4KiB = 4096, // 4 KiB alignment
    ESP_EXT_PART_ALIGN_1MiB = (1024 * 1024), // 1 MiB alignment
} esp_ext_part_align_t;

typedef enum __attribute__((packed))
{
    ESP_EXT_PART_TYPE_NONE = 0x00,
    ESP_EXT_PART_TYPE_FAT12,
    ESP_EXT_PART_TYPE_FAT16, /*!< FAT16 with LBA addressing */
    ESP_EXT_PART_TYPE_FAT32, /*!< FAT32 with LBA addressing */
    ESP_EXT_PART_TYPE_LITTLEFS, /*!< Possibly LittleFS (MBR CHS field => LittleFS block size hack) */
// Note: The following types are not supported, but we can return a type for them
    ESP_EXT_PART_TYPE_LINUX_ANY, /*!< Linux partition (any type) */
    ESP_EXT_PART_TYPE_EXFAT_OR_NTFS, /*!< Not supported, but we can return a type for it */
    ESP_EXT_PART_TYPE_GPT_PROTECTIVE_MBR, /*!< Not supported, but we can return a type for it */
} esp_ext_part_type_known_t;

typedef enum {
    ESP_EXT_PART_FLAG_NONE = 0,
    ESP_EXT_PART_FLAG_ACTIVE = 1 << 0,  /*!< Active / bootable partition */
    ESP_EXT_PART_FLAG_EXTRA = 1 << 1, /*!< Additional information stored in `extra` field (e.g. LittleFS block size stored in CHS hack) */
} esp_ext_part_flags_t;

typedef enum {
    ESP_EXT_PART_LIST_FLAG_NONE = 0,
    ESP_EXT_PART_LIST_FLAG_READ_ONLY = 1 << 0, /*!< Read-only partition list */
} esp_ext_part_list_flags_t;

typedef enum {
    ESP_EXT_PART_LIST_SIGNATURE_MBR, /*!< MBR signature type */
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
    esp_ext_part_sector_size_t sector_size; /*!< Sector size (storage medium property) */
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
esp_err_t esp_ext_part_list_insert(esp_ext_part_list_t *part_list, esp_ext_part_list_item_t *item);

/**
 * @brief Deep copy an external partition list.
 *
 * This function creates a deep copy of the source partition list into the destination partition list.
 * It allocates memory for the destination list and copies all items, including their labels.
 *
 * @note This function is not thread-safe.
 *
 * @param[out] dst Pointer to the destination partition list structure (must be allocated before but not initialized, i.e. "empty").
 * @param[in] src Pointer to the source partition list structure to copy from.
 *
 * @return
 *     - ESP_OK: Deep copy was successful.
 *     - ESP_ERR_INVALID_ARG: `dst` or `src` is NULL.
 *     - ESP_ERR_NO_MEM: Memory allocation failed.
 */
esp_err_t esp_ext_part_list_deep_copy(esp_ext_part_list_t *dst, esp_ext_part_list_t *src);

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
 * @brief Get the signature of an external partition list.
 *
 * This function retrieves the disk signature or identifier from the partition list.
 *
 * @param[in] part_list Pointer to the partition list structure.
 * @param[out] signature Pointer to a buffer where the signature will be stored.
 *
 * @return
 *     - ESP_OK: Signature retrieval was successful.
 *     - ESP_ERR_INVALID_ARG: `part_list` or signature is NULL.
 */
esp_err_t esp_ext_part_list_signature_get(esp_ext_part_list_t *part_list, void *signature);

/**
 * @brief Set the signature of an external partition list.
 *
 * This function sets the disk signature or identifier for the partition list.
 *
 * @param[in] part_list Pointer to the partition list structure.
 * @param[in] signature Pointer to the signature data to set.
 * @param[in] type      Type of the signature (e.g., MBR).
 *
 * @return
 *     - ESP_OK: Signature was successfully set.
 *     - ESP_ERR_INVALID_ARG: `part_list` or `signature` is NULL.
 *     - ESP_ERR_NOT_SUPPORTED: Unsupported signature type.
 */
esp_err_t esp_ext_part_list_signature_set(esp_ext_part_list_t *part_list, const void *signature, esp_ext_part_signature_type_t type);

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
/**
 * @brief Read a aprtition table and from a block device handle and parse it.
 *
 * This function reads the partition table from the specified block device and populates the provided partition list structure.
 * The type of partition table to read is specified by the 'type' parameter.
 * Additional arguments for parsing can be provided through the 'extra_args' parameter.
 *
 * @note This function is not thread-safe.
 *
 * @param[in] handle       Block device handle to read from.
 * @param[out] part_list   Pointer to the partition list structure to populate from the partition table.
 * @param[in] type         Type of partition table to read (e.g., MBR).
 * @param[in] extra_args   Pointer to additional arguments for parsing dependent on the partition type (optional, can be NULL).
 *
 * @return
 *     - ESP_OK: Partition list was successfully loaded.
 *     - ESP_ERR_INVALID_ARG: `handle` or `part_list` is NULL.
 *     - ESP_ERR_NOT_SUPPORTED: Unsupported partition table type.
 *     - ESP_ERR_NO_MEM: Memory allocation failed.
 *     - propagated errors from BDL operations or partition table parsing functions.
 */
esp_err_t esp_ext_part_list_bdl_read(esp_blockdev_handle_t handle, esp_ext_part_list_t *part_list, esp_ext_part_signature_type_t type, void *extra_args);

/**
 * @brief Generate a partition table and write it to a block device handle.
 *
 * This function writes the provided partition list to the specified block device.
 * The type of partition table to write is specified by the 'type' parameter.
 * Additional arguments for generation can be provided through the 'extra_args' parameter.
 *
 * @note This function is not thread-safe.
 *
 * @param[in] handle       Block device handle to write to.
 * @param[in] part_list    Pointer to the partition list structure generate the partition table from.
 * @param[in] type         Type of partition table to write (e.g., MBR).
 * @param[in] extra_args   Pointer to additional arguments for generation dependent on the partition type (optional, can be NULL).
 *
 * @return
 *     - ESP_OK: Partition list was successfully written.
 *     - ESP_ERR_INVALID_ARG: `handle` or `part_list` is NULL.
 *     - ESP_ERR_NOT_SUPPORTED: Unsupported partition table type.
 *     - ESP_ERR_NO_MEM: Memory allocation failed.
 *     - propagated errors from BDL operations or partition table generation functions.
 */
esp_err_t esp_ext_part_list_bdl_write(esp_blockdev_handle_t handle, esp_ext_part_list_t *part_list, esp_ext_part_signature_type_t type, void *extra_args);
#endif // (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))

#ifdef __cplusplus
}
#endif
