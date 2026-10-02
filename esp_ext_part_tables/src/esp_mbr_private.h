/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_mbr.h"

/**
 * True if `mbr` looks like a real MBR: boot signature present and every partition
 * entry's status byte is 0x00 or 0x80. The status check tells an MBR apart from a
 * volume boot record (e.g. FAT on a medium without a partition table), which also
 * ends in 0x55AA but has boot code where the partition entries would be.
 */
bool esp_mbr_is_valid(const esp_mbr_t *mbr);

/** Smallest and largest LittleFS block size accepted in the MBR CHS-start field. */
#define ESP_MBR_LITTLEFS_BLOCK_SIZE_MIN 128
#define ESP_MBR_LITTLEFS_BLOCK_SIZE_MAX (1024 * 1024)

/**
 * True if `val` is a usable LittleFS block size: a power of two in
 * [ESP_MBR_LITTLEFS_BLOCK_SIZE_MIN, ESP_MBR_LITTLEFS_BLOCK_SIZE_MAX]. Real CHS bytes
 * written by other tools into a 0xC3 entry almost never satisfy this.
 */
bool esp_mbr_littlefs_block_size_valid(uint64_t val);

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
#include "esp_blockdev.h"

/**
 * Size of the first I/O unit of the device that holds the MBR: at least ESP_MBR_SIZE
 * and a multiple of every relevant geometry granularity (read_size, write_size when
 * `for_write`, and erase_size when `for_write` and the device needs erase-before-write).
 *
 * @return ESP_OK, or ESP_ERR_NOT_SUPPORTED if the granularities have no common
 *         multiple handled here or the unit does not fit on the device.
 */
esp_err_t esp_mbr_bdl_io_unit(esp_blockdev_handle_t handle, bool for_write, size_t *out_unit);

/**
 * Allocate a buffer of the device's read I/O unit and read the first unit into it.
 * On success the caller frees `*out_buf`.
 */
esp_err_t esp_mbr_bdl_read_first_unit(esp_blockdev_handle_t handle, bool for_write, uint8_t **out_buf, size_t *out_unit);
#endif // (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
