/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Winbond-specific ECC status decoding: C0h ECC-1:0 for the 1-bit (Hamming) parts, plus
 * feature register 30h MBF for the KV series. Pure functions, built on all targets so host
 * tests can exercise them. */

#include <stdbool.h>
#include <stdint.h>
#include "nand_device_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Winbond 1-bit (Hamming) ECC decode, C0h ECC-1:0 only (W25N512G, W25N01GV/GW/JW).
 *
 * 00b: no errors; 01b: exactly 1 bit corrected. 10b (2-bit error in one page) and 11b (2-bit
 * errors in multiple pages, continuous read only) both map to NAND_ECC_NOT_CORRECTED.
 *
 * @param raw_status_c0  Raw C0h status byte.
 * @return Decoded ECC status.
 */
nand_ecc_status_t nand_wb_ecc_decode_1bit_strength(uint8_t raw_status_c0);

/**
 * @brief Check whether feature register 30h (MBF) is needed to decode this C0h status.
 *
 * @param status_c0  Raw C0h status byte.
 * @return true if C0h ECC-1:0 is 01b or 11b (corrected, at-or-below / above the BFD threshold).
 */
bool nand_wb_kv_ecc_needs_mbf(uint8_t status_c0);

/**
 * @brief Winbond W25N02KV ECC-1:0 + MBF decode.
 *
 * ECC-1:0=00b: no errors; 10b: not correctable. ECC-1:0=01b/11b: MBF (register 30h bits [7:4],
 * maximum bit flips in any sector of the page) is read and becomes the trusted source:
 * 0 -> NAND_ECC_OK, 1-8 -> exact count, 1111b (>8) -> NAND_ECC_NOT_CORRECTED,
 * undefined (1001b-1110b) -> NAND_ECC_INVALID.
 *
 * @param raw_status_c0  Raw C0h status byte.
 * @param raw_reg_30h    Raw feature register 30h byte (don't-care unless
 *                       nand_wb_kv_ecc_needs_mbf() is true).
 * @return Decoded ECC status.
 */
nand_ecc_status_t nand_wb_kv_ecc_decode(uint8_t raw_status_c0, uint8_t raw_reg_30h);

#ifdef __cplusplus
}
#endif
