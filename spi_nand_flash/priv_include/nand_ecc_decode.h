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

/** @brief Extract the 2-bit ECC status field (bits [5:4]) used by C0h, and by GigaDevice F0h. */
#define NAND_ECC_2BIT_FIELD(reg)        (((reg) >> 4) & 0x3u)

/** @brief 2-bit ECC status values with the same meaning on every vendor. 01b and 11b differ. */
#define NAND_ECC_2BIT_NO_ERROR          0b00
#define NAND_ECC_2BIT_UNCORRECTABLE     0b10

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
 * The strength_N suffix is the number of bits the chip's internal ECC can correct per ECC step.
 *
 * @param dev        Device handle (unused; present to match nand_ecc_decode_fn).
 * @param status_c0  Raw C0h status byte.
 * @param[out] out   Decoded ECC status.
 * @return ESP_OK always; no extra register reads are needed.
 */
esp_err_t nand_ecc_decode_2bit_strength_4(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out);

/**
 * @brief Same as nand_ecc_decode_2bit_strength_4(), for chips with 8-bit internal ECC strength.
 *
 * 01b maps to 1-7; 11b maps to exactly 8.
 *
 * @param dev        Device handle (unused; present to match nand_ecc_decode_fn).
 * @param status_c0  Raw C0h status byte.
 * @param[out] out   Decoded ECC status.
 * @return ESP_OK always; no extra register reads are needed.
 */
esp_err_t nand_ecc_decode_2bit_strength_8(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out);

/**
 * @brief Decode a 2-bit ECCS field (C0h bits [5:4]) for chips with 1-bit (Hamming) internal ECC.
 *
 * 00b: no errors; 01b: exactly 1 bit corrected. 10b and 11b both map to NAND_ECC_NOT_CORRECTED
 * (Winbond W25N512G, W25N01GV/GW/JW: 11b is a 2-bit error in continuous read mode).
 *
 * @param dev        Device handle (unused; present to match nand_ecc_decode_fn).
 * @param status_c0  Raw C0h status byte.
 * @param[out] out   Decoded ECC status.
 * @return ESP_OK always; no extra register reads are needed.
 */
esp_err_t nand_ecc_decode_2bit_strength_1(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out);

/**
 * @brief Decode the XTX XT26G08D 4-bit ECCS field (C0h bits [7:4]).
 *
 * ECCS1:0 (bits [5:4]): 00b no errors, 10b not correctable, 11b exactly 8 corrected.
 * When ECCS1:0 is 01b, ECCS3:2 (bits [7:6]) gives the count: <=4, 5, 6 or 7.
 *
 * @param dev        Device handle (unused; present to match nand_ecc_decode_fn).
 * @param status_c0  Raw C0h status byte.
 * @param[out] out   Decoded ECC status.
 * @return ESP_OK always; no extra register reads are needed.
 */
esp_err_t nand_ecc_decode_xtx(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out);

/**
 * @brief Check whether a 2-bit ECCS field (C0h bits [5:4]) reports a correction (01b or 11b).
 *
 * For chips that report the exact count in a separate register (Macronix ECCSR, Winbond KV
 * MBF), this tells the caller whether that register needs to be read.
 *
 * @param status_c0  Raw C0h status byte.
 * @return true if ECCS is 01b or 11b.
 */
bool nand_ecc_2bit_reports_correction(uint8_t status_c0);

/**
 * @brief Decode a 2-bit ECCS field (C0h bits [5:4]) together with an exact error count read
 *        from a vendor register.
 *
 * ECCS 00b: no errors; 10b: not correctable; `count` is not used. ECCS 01b/11b: `count` is
 * the trusted source: 0 -> NAND_ECC_OK, 1-8 -> exact count, 1111b (>8) ->
 * NAND_ECC_NOT_CORRECTED, undefined (1001b-1110b) -> NAND_ECC_INVALID. Used for Macronix
 * ECCSR[3:0] and Winbond KV MBF (register 30h [7:4]).
 *
 * @param status_c0  Raw C0h status byte.
 * @param count      4-bit error count, already extracted from the vendor register (don't-care
 *                   unless nand_ecc_2bit_reports_correction() is true).
 * @return Decoded ECC status.
 */
nand_ecc_status_t nand_ecc_decode_2bit_with_count(uint8_t status_c0, uint8_t count);

#ifdef __cplusplus
}
#endif
