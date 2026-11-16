/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* XTX-specific ECC status decoding (4-bit ECCS in C0h). Pure function, built on all targets
 * so host tests can exercise it. */

#include <stdint.h>
#include "nand_device_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief XTX XT26G08D 4-bit ECCS (C0h [7:4]). ECCS1:0: 00b OK, 10b not corrected, 11b
 *        exactly 8. When ECCS1:0 is 01b, ECCS3:2 gives the count: <=4, 5, 6 or 7.
 *
 * @param status_c0  Raw C0h status byte.
 * @return Decoded ECC status.
 */
nand_ecc_status_t nand_xtx_ecc_decode(uint8_t status_c0);

#ifdef __cplusplus
}
#endif
