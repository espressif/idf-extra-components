/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_winbond_ecc_decode.h"

/* C0h ECC-1:0 occupies bits [5:4]. */
#define WB_ECC_S(reg)  (((reg) >> 4) & 0x3u)

#define WB_ECC_S_NO_ERROR        0b00
#define WB_ECC_S_AT_OR_BELOW_BFD 0b01    /* corrected, count <= bit flip detection threshold */
#define WB_ECC_S_UNCORRECTABLE   0b10
#define WB_ECC_S_ABOVE_BFD       0b11    /* corrected, count > bit flip detection threshold */

/* Feature register 30h: MBF [7:4] = maximum bit flips in any sector of the page,
 * MFS [2:0] = that sector. MBF 0-8 is the exact count, 1111b is >8; others are undefined. */
#define WB_REG30_MBF(reg)  (((reg) >> 4) & 0xFu)

static const nand_ecc_status_t s_wb_mbf_map[16] = {
    [0]  = NAND_ECC_INVALID,            /* ECC-1:0 says corrected, MBF says 0 */
    [1]  = NAND_ECC_1_BIT_CORRECTED,
    [2]  = NAND_ECC_2_BITS_CORRECTED,
    [3]  = NAND_ECC_3_BITS_CORRECTED,
    [4]  = NAND_ECC_4_BITS_CORRECTED,
    [5]  = NAND_ECC_5_BITS_CORRECTED,
    [6]  = NAND_ECC_6_BITS_CORRECTED,
    [7]  = NAND_ECC_7_BITS_CORRECTED,
    [8]  = NAND_ECC_8_BITS_CORRECTED,
    [9]  = NAND_ECC_INVALID,            /* 9-14: undefined */
    [10] = NAND_ECC_INVALID,
    [11] = NAND_ECC_INVALID,
    [12] = NAND_ECC_INVALID,
    [13] = NAND_ECC_INVALID,
    [14] = NAND_ECC_INVALID,
    [15] = NAND_ECC_INVALID,            /* >8, contradicts ECC-1:0 "corrected" */
};

bool nand_wb_kv_ecc_needs_mbf(uint8_t status_c0)
{
    const uint8_t s = WB_ECC_S(status_c0);
    return s == WB_ECC_S_AT_OR_BELOW_BFD || s == WB_ECC_S_ABOVE_BFD;
}

nand_ecc_status_t nand_wb_kv_ecc_decode(uint8_t raw_status_c0, uint8_t raw_reg_30h)
{
    switch (WB_ECC_S(raw_status_c0)) {
    case WB_ECC_S_NO_ERROR:
        return NAND_ECC_OK;
    case WB_ECC_S_AT_OR_BELOW_BFD:
    case WB_ECC_S_ABOVE_BFD:
        /* The BFD split is ignored: MBF gives the exact count either way. */
        return s_wb_mbf_map[WB_REG30_MBF(raw_reg_30h)];
    case WB_ECC_S_UNCORRECTABLE:
        return NAND_ECC_NOT_CORRECTED;
    }
    return NAND_ECC_INVALID;    /* unreachable: WB_ECC_S() is 2 bits wide */
}
