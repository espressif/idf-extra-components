/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_xtx_ecc_decode.h"

/* C0h ECCS1:0 occupies bits [5:4], ECCS3:2 bits [7:6]. */
#define XTX_ECCS_LO(reg)  (((reg) >> 4) & 0x3u)
#define XTX_ECCS_HI(reg)  (((reg) >> 6) & 0x3u)

#define XTX_ECCS_NO_ERROR       0b00
#define XTX_ECCS_CORRECTED      0b01    /* corrected, <= 7; ECCS3:2 gives the count */
#define XTX_ECCS_UNCORRECTABLE  0b10
#define XTX_ECCS_8_CORRECTED    0b11

/* ECCS3:2 when ECCS1:0 is 01b. Don't-care for the other ECCS1:0 values. */
static const nand_ecc_status_t s_xtx_corrected_map[4] = {
    [0] = NAND_ECC_1_TO_4_BITS_CORRECTED,
    [1] = NAND_ECC_5_BITS_CORRECTED,
    [2] = NAND_ECC_6_BITS_CORRECTED,
    [3] = NAND_ECC_7_BITS_CORRECTED,
};

nand_ecc_status_t nand_xtx_ecc_decode(uint8_t raw_status_c0)
{
    switch (XTX_ECCS_LO(raw_status_c0)) {
    case XTX_ECCS_NO_ERROR:
        return NAND_ECC_OK;
    case XTX_ECCS_CORRECTED:
        return s_xtx_corrected_map[XTX_ECCS_HI(raw_status_c0)];
    case XTX_ECCS_UNCORRECTABLE:
        return NAND_ECC_NOT_CORRECTED;
    case XTX_ECCS_8_CORRECTED:
        return NAND_ECC_8_BITS_CORRECTED;
    }
    return NAND_ECC_INVALID;    /* unreachable: XTX_ECCS_LO() is 2 bits wide */
}
