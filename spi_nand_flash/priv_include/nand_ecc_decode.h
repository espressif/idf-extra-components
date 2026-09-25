/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "nand_device_types.h"
#include "spi_nand_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief nand_ecc_max_bits_corrected() result when the status carries no correction count. */
#define NAND_ECC_BITS_UNKNOWN UINT8_MAX

/**
 * @brief Worst-case corrected-bit count for a status class (range ceiling).
 *
 * Refresh compares this to the tunable threshold. Using max (not min) may
 * refresh early if the threshold sits inside a range; using min can skip a
 * needed refresh.
 *
 * @return Bits corrected (0 for NAND_ECC_OK), or NAND_ECC_BITS_UNKNOWN when no
 *         correction count applies (not corrected, invalid, or not a status).
 */
static inline uint8_t nand_ecc_max_bits_corrected(nand_ecc_status_t status)
{
    switch (status) {
    case NAND_ECC_1_TO_3_BITS_CORRECTED:
        return 3;
    case NAND_ECC_4_TO_6_BITS_CORRECTED:
        return 6;
    case NAND_ECC_7_8_BITS_CORRECTED:
        return 8;
    case NAND_ECC_1_TO_4_BITS_CORRECTED:
        return 4;
    case NAND_ECC_1_TO_7_BITS_CORRECTED:
        return 7;
    case NAND_ECC_1_BIT_CORRECTED:
        return 1;
    case NAND_ECC_2_BITS_CORRECTED:
        return 2;
    case NAND_ECC_3_BITS_CORRECTED:
        return 3;
    case NAND_ECC_4_BITS_CORRECTED:
        return 4;
    case NAND_ECC_5_BITS_CORRECTED:
        return 5;
    case NAND_ECC_6_BITS_CORRECTED:
        return 6;
    case NAND_ECC_7_BITS_CORRECTED:
        return 7;
    case NAND_ECC_8_BITS_CORRECTED:
        return 8;
    case NAND_ECC_OK:
        return 0;
    case NAND_ECC_NOT_CORRECTED:
    case NAND_ECC_INVALID:
    case NAND_ECC_MAX:
        return NAND_ECC_BITS_UNKNOWN;
    }
    /* No default: above, so -Wswitch flags any nand_ecc_status_t value missing here. */
    return NAND_ECC_BITS_UNKNOWN;
}

/**
 * @brief Check whether the last read's corrected-bit count calls for a data refresh.
 *
 * Statuses without a bit count (not corrected, invalid) never exceed the threshold.
 *
 * @param ecc  ECC data holding the last status and the refresh threshold.
 * @return true if the corrected-bit count meets or exceeds the threshold.
 */
static inline bool nand_ecc_exceeds_data_refresh_threshold(const nand_ecc_data_t *ecc)
{
    uint8_t bits = nand_ecc_max_bits_corrected(ecc->ecc_corrected_bits_status);
    return bits != NAND_ECC_BITS_UNKNOWN && bits >= ecc->ecc_data_refresh_threshold;
}

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
 *        for chips with 4-bit internal ECC strength.
 *
 * 01b means corrected below the maximum (no count), so it maps to 1-3; 11b maps to exactly 4.
 *
 * @param dev        Device handle (unused; present to match nand_ecc_decode_fn).
 * @param status_c0  Raw C0h status byte.
 * @param[out] out   Decoded ECC status.
 * @return ESP_OK always; no extra register reads are needed.
 */
esp_err_t nand_ecc_decode_2bit_4bit_strength(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out);

/**
 * @brief Same as nand_ecc_decode_2bit_4bit_strength(), for chips with 8-bit internal ECC strength.
 *
 * 01b maps to 1-7; 11b maps to exactly 8.
 *
 * @param dev        Device handle (unused; present to match nand_ecc_decode_fn).
 * @param status_c0  Raw C0h status byte.
 * @param[out] out   Decoded ECC status.
 * @return ESP_OK always; no extra register reads are needed.
 */
esp_err_t nand_ecc_decode_2bit_8bit_strength(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out);

/**
 * @brief Map a 4-bit error count field, read from a vendor register, to a status.
 *
 * Layout shared by Macronix ECCSR and Winbond MBF: 0-8 is the exact error count, 1111b is
 * ">8 errors", other values are undefined. Once the caller has read this register, it is the
 * trusted source and the C0h ECC status is not consulted again.
 *
 * @param count  Error count field, already extracted from the vendor register (the caller
 *               does the register read and the masking).
 * @return NAND_ECC_OK (0), NAND_ECC_1_BIT_CORRECTED .. NAND_ECC_8_BITS_CORRECTED (1-8),
 *         NAND_ECC_NOT_CORRECTED (1111b), or NAND_ECC_INVALID (undefined values).
 */
nand_ecc_status_t nand_ecc_status_from_vendor_bit_count(uint8_t count);

#ifdef __cplusplus
}
#endif
