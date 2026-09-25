/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_macronix_ecc_decode.h"

/* C0h ECC_S1:S0 occupies bits [5:4]. */
#define MX_ECC_S(reg)  (((reg) >> 4) & 0x3u)

#define MX_ECC_S_NO_ERROR        0b00
#define MX_ECC_S_BELOW_BFT       0b01    /* corrected, count < bit flip threshold */
#define MX_ECC_S_UNCORRECTABLE   0b10
#define MX_ECC_S_AT_OR_ABOVE_BFT 0b11    /* corrected, count >= bit flip threshold */

/* 7Ch ECCSR: bits [3:0] current page, bits [7:4] accumulated pages (continuous read only).
 * 0-8 is the exact error count, 1111b is >8; other values are undefined. */
#define MX_ECCSR_CURRENT_PAGE(eccsr)  ((eccsr) & 0xFu)

static const nand_ecc_status_t s_mx_eccsr_map[16] = {
    [0]  = NAND_ECC_INVALID,            /* ECC_S says corrected, ECCSR says 0 */
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
    [15] = NAND_ECC_INVALID,            /* >8, contradicts ECC_S "corrected" */
};

bool nand_mx_ecc_needs_eccsr(uint8_t status_c0)
{
    const uint8_t s = MX_ECC_S(status_c0);
    return s == MX_ECC_S_BELOW_BFT || s == MX_ECC_S_AT_OR_ABOVE_BFT;
}

nand_ecc_status_t nand_mx_ecc_decode(uint8_t raw_status_c0, uint8_t raw_eccsr)
{
    switch (MX_ECC_S(raw_status_c0)) {
    case MX_ECC_S_NO_ERROR:
        return NAND_ECC_OK;
    case MX_ECC_S_BELOW_BFT:
    case MX_ECC_S_AT_OR_ABOVE_BFT:
        /* The BFT split is ignored: ECCSR gives the exact count either way. */
        return s_mx_eccsr_map[MX_ECCSR_CURRENT_PAGE(raw_eccsr)];
    case MX_ECC_S_UNCORRECTABLE:
        return NAND_ECC_NOT_CORRECTED;
    }
    return NAND_ECC_INVALID;    /* unreachable: MX_ECC_S() is 2 bits wide */
}
