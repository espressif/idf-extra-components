/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* XTX-specific ECC status decoding (C0h ECCS3:0). Pure functions, built on all targets so
 * host tests can exercise them. */

#include <stdint.h>
#include "nand_device_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief XTX XT26G08D ECC decode, 4-bit ECCS field in C0h bits [7:4].
 *
 * ECCS1:0 (bits [5:4]): 00b no errors, 10b not correctable, 11b exactly 8 corrected.
 * When ECCS1:0 is 01b, ECCS3:2 (bits [7:6]) gives the count: <=4, 5, 6 or 7.
 *
 * @param raw_status_c0  Raw C0h status byte.
 * @return Decoded ECC status.
 */
nand_ecc_status_t nand_xtx_ecc_decode(uint8_t raw_status_c0);

#ifdef __cplusplus
}
#endif
