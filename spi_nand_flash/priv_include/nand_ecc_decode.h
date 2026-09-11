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

#ifdef __cplusplus
}
#endif
