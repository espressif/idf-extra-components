/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "nand_device_types.h"
#include "spi_nand_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decode a 2-bit ECCS field (C0h bits [5:4]). Default decoder for most chips.
 *
 * @param dev        Device handle (unused; present to match nand_ecc_decode_fn).
 * @param status_c0  Raw C0h status byte.
 * @param[out] out   Decoded ECC status.
 * @return ESP_OK always; no extra register reads are needed.
 */
esp_err_t nand_ecc_decode_2bit(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out);

/**
 * @brief Decode a 3-bit ECCS field (C0h bits [6:4]).
 *
 * Reserved patterns (100b, 110b, 111b) map to NAND_ECC_INVALID.
 *
 * @param dev        Device handle (unused; present to match nand_ecc_decode_fn).
 * @param status_c0  Raw C0h status byte.
 * @param[out] out   Decoded ECC status.
 * @return ESP_OK always; no extra register reads are needed.
 */
esp_err_t nand_ecc_decode_3bit(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out);

/**
 * @brief Decode a 2-bit ECCS field (C0h bits [5:4]) where 11b means "maximum corrected",
 *        for chips with 8-bit internal ECC strength.
 *
 * 01b means corrected below the maximum (no count), so it maps to 1-7; 11b maps to exactly 8.
 * Used by Zetta ZD35Q1GC.
 *
 * @param dev        Device handle (unused; present to match nand_ecc_decode_fn).
 * @param status_c0  Raw C0h status byte.
 * @param[out] out   Decoded ECC status.
 * @return ESP_OK always; no extra register reads are needed.
 */
esp_err_t nand_ecc_decode_2bit_8bit_strength(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out);

#ifdef __cplusplus
}
#endif
