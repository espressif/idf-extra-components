/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* GigaDevice-specific ECC status decoding (C0h ECCS + F0h ECCSE). Pure functions,
 * built on all targets so host tests can exercise them. */

#include <stdbool.h>
#include <stdint.h>
#include "nand_device_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check whether F0h ECCSE is needed to decode this C0h status.
 *
 * @param status_c0  Raw C0h status byte.
 * @return true if C0h ECCS is 01b, the only value where F0h ECCSE is meaningful.
 */
bool nand_gd_ecc_needs_status_ext(uint8_t status_c0);

/**
 * @brief GigaDevice ECCS+ECCSE decode for chips with 8-bit/528B internal ECC strength.
 *
 * ECCS=01b: F0h ECCSE gives 1-4 (single ceiling), 5, 6 or 7 bits corrected.
 * ECCS=11b: exactly 8 bits corrected. ECCS=00b: no errors; 10b: not correctable.
 *
 * @param raw_status_c0  Raw C0h status byte.
 * @param raw_status_f0  Raw F0h status byte (don't-care unless ECCS is 01b).
 * @return Decoded ECC status.
 */
nand_ecc_status_t nand_gd_ecc_decode_8bit_strength(uint8_t raw_status_c0, uint8_t raw_status_f0);

/**
 * @brief GigaDevice ECCS+ECCSE decode for chips with 4-bit/528B internal ECC strength.
 *
 * ECCS=01b: F0h ECCSE gives exactly 1, 2, 3 or 4 bits corrected.
 * ECCS=11b: reserved, maps to NAND_ECC_INVALID. ECCS=00b: no errors; 10b: not correctable.
 *
 * @param raw_status_c0  Raw C0h status byte.
 * @param raw_status_f0  Raw F0h status byte (don't-care unless ECCS is 01b).
 * @return Decoded ECC status.
 */
nand_ecc_status_t nand_gd_ecc_decode_4bit_strength(uint8_t raw_status_c0, uint8_t raw_status_f0);

#ifdef __cplusplus
}
#endif
