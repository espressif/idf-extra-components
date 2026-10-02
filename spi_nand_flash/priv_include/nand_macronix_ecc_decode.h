/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Macronix-specific ECC status decoding (C0h ECC_S + 7Ch ECCSR). Pure functions,
 * built on all targets so host tests can exercise them. */

#include <stdbool.h>
#include <stdint.h>
#include "nand_device_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check whether the 7Ch ECCSR byte is needed to decode this C0h status.
 *
 * @param status_c0  Raw C0h status byte.
 * @return true if C0h ECC_S is 01b or 11b (corrected, below or at/above the BFT threshold).
 */
bool nand_mx_ecc_needs_eccsr(uint8_t status_c0);

/**
 * @brief Macronix MX35LFxGE4AD ECC_S + ECCSR decode.
 *
 * ECC_S=00b: no errors; 10b: not correctable. ECC_S=01b/11b: ECCSR is read and becomes the
 * trusted source. ECCSR bits [3:0] (current page): 0 -> NAND_ECC_OK, 1-8 -> exact count,
 * 1111b (>8) -> NAND_ECC_NOT_CORRECTED, undefined (1001b-1110b) -> NAND_ECC_INVALID.
 *
 * @param raw_status_c0  Raw C0h status byte.
 * @param raw_eccsr      Raw byte from the Read ECC Status (7Ch) command (don't-care unless
 *                       nand_mx_ecc_needs_eccsr() is true).
 * @return Decoded ECC status.
 */
nand_ecc_status_t nand_mx_ecc_decode(uint8_t raw_status_c0, uint8_t raw_eccsr);

#ifdef __cplusplus
}
#endif
