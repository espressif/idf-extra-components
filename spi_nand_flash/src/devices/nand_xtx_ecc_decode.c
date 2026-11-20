/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_xtx_ecc_decode.h"
#include "nand_ecc_decode.h"

/* XTX ECCS3:0, indexed by ECCS3:2 (bits [7:6]) when ECCS1:0 (bits [5:4]) is 01b. */
#define XTX_ECCS_HI(reg)  (((reg) >> 6) & 0b11u)
#define XTX_ECCS_CORRECTED    0b01    /* corrected, <= 7; ECCS3:2 gives the count */
#define XTX_ECCS_8_CORRECTED  0b11

static const nand_ecc_status_t s_xtx_ecc_corrected_map[4] = {
    [0] = NAND_ECC_1_TO_4_BITS_CORRECTED,
    [1] = NAND_ECC_5_BITS_CORRECTED,
    [2] = NAND_ECC_6_BITS_CORRECTED,
    [3] = NAND_ECC_7_BITS_CORRECTED,
};

nand_ecc_status_t nand_xtx_ecc_decode(uint8_t status_c0)
{
    switch (NAND_ECC_2BIT_FIELD(status_c0)) {
    case NAND_ECC_2BIT_NO_ERROR:
        return NAND_ECC_OK;
    case XTX_ECCS_CORRECTED:
        return s_xtx_ecc_corrected_map[XTX_ECCS_HI(status_c0)];
    case NAND_ECC_2BIT_UNCORRECTABLE:
        return NAND_ECC_NOT_CORRECTED;
    case XTX_ECCS_8_CORRECTED:
        return NAND_ECC_8_BITS_CORRECTED;
    }
    return NAND_ECC_INVALID;    /* unreachable: NAND_ECC_2BIT_FIELD() is 2 bits wide */
}
