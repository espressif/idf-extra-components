/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_winbond_ecc_decode.h"
#include "nand_ecc_decode.h"

/* C0h ECC-1:0 occupies bits [5:4]. */
#define WB_ECC_S(reg)  (((reg) >> 4) & 0x3u)

#define WB_ECC_S_NO_ERROR        0b00
#define WB_ECC_S_AT_OR_BELOW_BFD 0b01    /* corrected, count <= bit flip detection threshold */
#define WB_ECC_S_UNCORRECTABLE   0b10
#define WB_ECC_S_ABOVE_BFD       0b11    /* corrected, count > bit flip detection threshold */

/* Feature register 30h: MBF [7:4] = maximum bit flips in any sector of the page,
 * MFS [2:0] = that sector. MBF 0-8 is the exact count, 1111b is >8; others are undefined.
 * MBF is only read when ECC-1:0 reports a correction, and once read it is the trusted
 * source: 0 -> OK, 1-8 -> exact count, 1111b -> not corrected, undefined -> invalid. */
#define WB_REG30_MBF(reg)  (((reg) >> 4) & 0xFu)

/* W25N01GV-style 1-bit (Hamming) ECC: 01b is exactly 1 bit corrected. 10b is a 2-bit error in
 * one page, 11b is 2-bit errors in multiple pages (continuous read only); both uncorrectable. */
static const nand_ecc_status_t s_wb_1bit_strength_map[4] = {
    [WB_ECC_S_NO_ERROR]        = NAND_ECC_OK,
    [WB_ECC_S_AT_OR_BELOW_BFD] = NAND_ECC_1_BIT_CORRECTED,
    [WB_ECC_S_UNCORRECTABLE]   = NAND_ECC_NOT_CORRECTED,
    [WB_ECC_S_ABOVE_BFD]       = NAND_ECC_NOT_CORRECTED,
};

nand_ecc_status_t nand_wb_ecc_decode_1bit_strength(uint8_t raw_status_c0)
{
    return s_wb_1bit_strength_map[WB_ECC_S(raw_status_c0)];
}

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
        /* The BFD split is ignored: MBF is read and trusted either way. */
        return nand_ecc_status_from_vendor_bit_count(WB_REG30_MBF(raw_reg_30h));
    case WB_ECC_S_UNCORRECTABLE:
        return NAND_ECC_NOT_CORRECTED;
    }
    return NAND_ECC_INVALID;    /* unreachable: WB_ECC_S() is 2 bits wide */
}
