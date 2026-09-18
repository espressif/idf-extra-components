/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "nand_device_types.h"
#include "spi_nand_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Map C0h ECC bits [5:4] through the 2-bit lookup table into nand_ecc_status_t.
 */
nand_ecc_status_t nand_ecc_pack_decode_2bit(uint8_t status_c0);

/**
 * Map C0h ECC bits [6:4] through the 3-bit lookup table into nand_ecc_status_t.
 * Reserved patterns (4, 6, 7) map to NAND_ECC_MAX.
 */
nand_ecc_status_t nand_ecc_pack_decode_3bit(uint8_t status_c0);

/**
 * True when C0h ECCS is 01b, the only prefix where F0h ECCSE is meaningful.
 */
bool nand_ecc_gd_needs_status_ext(uint8_t status_c0);

/**
 * GigaDevice ECCS+ECCSE prefix dispatch into nand_ecc_status_t.
 *
 * @param raw_status_c0  Raw C0h status byte.
 * @param raw_status_f0  Raw F0h status byte (don't-care unless ECCS is 01b).
 * @param f0_read_ok     True if F0h was read successfully. Ignored unless ECCS is 01b.
 *                       A failed F0h read maps to NAND_ECC_4_TO_6_BITS_CORRECTED
 *                       (force refresh, do not fail the page).
 */
nand_ecc_status_t nand_ecc_decode_gd_eccse(uint8_t raw_status_c0, uint8_t raw_status_f0, bool f0_read_ok);

/** Device-bound 2-bit decoder (default). */
nand_ecc_status_t nand_ecc_decode_2bit(spi_nand_flash_device_t *dev, uint8_t status_c0);

/** Device-bound 3-bit decoder (Micron, FM). */
nand_ecc_status_t nand_ecc_decode_3bit(spi_nand_flash_device_t *dev, uint8_t status_c0);

#ifdef __cplusplus
}
#endif
