/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Winbond KV-series ECC status decoding (C0h ECC-1:0 + feature register 30h MBF). Pure
 * functions, built on all targets so host tests can exercise them. */

#include <stdbool.h>
#include <stdint.h>
#include "nand_device_types.h"

#ifdef __cplusplus
extern "C" {
#endif

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
 * ECC-1:0=00b: no errors; 10b: not correctable. ECC-1:0=01b/11b: bits were corrected, and MBF
 * (register 30h bits [7:4], maximum bit flips in any sector of the page) gives the exact
 * count, 1-8. A count that contradicts ECC-1:0 (0, >8, or undefined) maps to NAND_ECC_INVALID.
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
