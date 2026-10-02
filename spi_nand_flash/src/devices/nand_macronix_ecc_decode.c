/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_macronix_ecc_decode.h"
#include "nand_ecc_decode.h"

/* C0h ECC_S1:S0 occupies bits [5:4]. */
#define MX_ECC_S(reg)  (((reg) >> 4) & 0x3u)

#define MX_ECC_S_NO_ERROR        0b00
#define MX_ECC_S_BELOW_BFT       0b01    /* corrected, count < bit flip threshold */
#define MX_ECC_S_UNCORRECTABLE   0b10
#define MX_ECC_S_AT_OR_ABOVE_BFT 0b11    /* corrected, count >= bit flip threshold */

/* 7Ch ECCSR: bits [3:0] current page, bits [7:4] accumulated pages (continuous read only).
 * 0-8 is the exact error count, 1111b is >8; other values are undefined.
 * ECCSR is only read when ECC_S reports a correction, and once read it is the trusted
 * source: 0 -> OK, 1-8 -> exact count, 1111b -> not corrected, undefined -> invalid. */
#define MX_ECCSR_CURRENT_PAGE(eccsr)  ((eccsr) & 0xFu)

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
        /* The BFT split is ignored: ECCSR is read and trusted either way. */
        return nand_ecc_status_from_vendor_bit_count(MX_ECCSR_CURRENT_PAGE(raw_eccsr));
    case MX_ECC_S_UNCORRECTABLE:
        return NAND_ECC_NOT_CORRECTED;
    }
    return NAND_ECC_INVALID;    /* unreachable: MX_ECC_S() is 2 bits wide */
}
