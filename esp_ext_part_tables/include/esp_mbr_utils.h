/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_ext_part_tables.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MBR_CHS_HEADS 255
#define MBR_CHS_SECTORS_PER_TRACK 63
#define MBR_CHS_MAX_CYLINDER 1023
#define MBR_CHS_MAX_HEAD 254
#define MBR_CHS_MAX_SECTOR 63

// Helper functions for MBR CHS conversion and LBA alignment

/**
 * @brief Set a 3-byte CHS array from a 24-bit value.
 *
 * @param[out] chs 3-byte array to store the CHS value.
 * @param[in]  val 24-bit value representing CHS.
 */
void esp_mbr_chs_arr_val_set(uint8_t chs[3], uint32_t val);

/**
 * @brief Get a 24-bit value from a 3-byte CHS array.
 *
 * @param[in] chs 3-byte array containing the CHS value.
 * @return 24-bit value representing CHS stored in `uint32_t`.
 */
uint32_t esp_mbr_chs_arr_val_get(const uint8_t chs[3]);

/**
 * @brief Convert an LBA value to a 3-byte CHS array.
 *
 * @param[out] chs 3-byte array to store the CHS value.
 * @param[in]  lba Logical Block Address to convert.
 */
void esp_mbr_lba_to_chs_arr(uint8_t chs[3], uint32_t lba);

/**
 * @brief Align an LBA value according to sector size and alignment requirements.
 *
 * @param[in] lba         Logical Block Address to align.
 * @param[in] sector_size Sector size enumeration.
 * @param[in] alignment   Alignment requirement enumeration.
 * @return Aligned LBA value.
 */
uint32_t esp_mbr_lba_align(uint32_t lba, esp_ext_part_sector_size_t sector_size, esp_ext_part_align_t alignment);

/**
 * @brief Generate default supported MBR partition types for a given internal type.
 *
 * @param[in] type Internal artition type to generate supported types from internal `esp_ext_part_type_known_t` enum.
 * @return MBR partition type code.
 */
uint8_t esp_mbr_generate_default_supported_partition_types(uint8_t type);

/**
 * @brief Parse default supported MBR partition types into internal types (`esp_ext_part_type_known_t`).
 *
 * @param[in]  type             MBR partition type code to parse.
 * @param[out] out_type_parsed  Pointer to store the parsed internal partition type (from `esp_ext_part_type_known_t`).
 * @return true if the partition type is known and supported, false otherwise.
 */
bool esp_mbr_parse_default_supported_partition_types(uint8_t type, uint8_t *out_type_parsed);

#ifdef __cplusplus
}
#endif
