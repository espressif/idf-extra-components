/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_gigadevice_ecc_decode.h"

/* C0h ECCS and F0h ECCSE both occupy bits [5:4]. */
#define GD_ECC_FIELD(reg)  (((reg) >> 4) & 0x3u)

/* C0h ECCS values. Only GD_ECCS_SEE_EXT needs F0h ECCSE to resolve the count;
 * the meaning of 11b depends on the chip's ECC strength. */
#define GD_ECCS_NO_ERROR       0b00
#define GD_ECCS_SEE_EXT        0b01
#define GD_ECCS_UNCORRECTABLE  0b10
#define GD_ECCS_11B            0b11

/* F0h ECCSE, valid only when C0h ECCS == 01b.
 * 8-bit/528B internal-ECC-strength family: collapses 1-4 into a ceiling,
 * resolves 5/6/7 exactly; ECCS=11b separately means exactly 8. */
static const nand_ecc_status_t s_gd_eccse_8bit_strength_map[4] = {
    [0] = NAND_ECC_1_TO_4_BITS_CORRECTED,
    [1] = NAND_ECC_5_BITS_CORRECTED,
    [2] = NAND_ECC_6_BITS_CORRECTED,
    [3] = NAND_ECC_7_BITS_CORRECTED,
};

/* 4-bit/528B internal-ECC-strength family: ECCSE resolves 1/2/3/4 exactly.
 * ECCS=11b is reserved/invalid for this family. */
static const nand_ecc_status_t s_gd_eccse_4bit_strength_map[4] = {
    [0] = NAND_ECC_1_BIT_CORRECTED,
    [1] = NAND_ECC_2_BITS_CORRECTED,
    [2] = NAND_ECC_3_BITS_CORRECTED,
    [3] = NAND_ECC_4_BITS_CORRECTED,
};

bool nand_gd_ecc_needs_status_ext(uint8_t status_c0)
{
    return GD_ECC_FIELD(status_c0) == GD_ECCS_SEE_EXT;
}

nand_ecc_status_t nand_gd_ecc_decode_8bit_strength(uint8_t raw_status_c0, uint8_t raw_status_f0)
{
    switch (GD_ECC_FIELD(raw_status_c0)) {
    case GD_ECCS_NO_ERROR:
        return NAND_ECC_OK;
    case GD_ECCS_SEE_EXT:
        /* F0h ECCSE reuses bits [5:4], same positions as C0h ECCS -
         * this is intentional per the datasheet, not a copy-paste of the C0h read. */
        return s_gd_eccse_8bit_strength_map[GD_ECC_FIELD(raw_status_f0)];
    case GD_ECCS_UNCORRECTABLE:
        return NAND_ECC_NOT_CORRECTED;
    case GD_ECCS_11B:
        return NAND_ECC_8_BITS_CORRECTED;
    }
    return NAND_ECC_INVALID;    /* unreachable: GD_ECC_FIELD() is 2 bits wide */
}

nand_ecc_status_t nand_gd_ecc_decode_4bit_strength(uint8_t raw_status_c0, uint8_t raw_status_f0)
{
    switch (GD_ECC_FIELD(raw_status_c0)) {
    case GD_ECCS_NO_ERROR:
        return NAND_ECC_OK;
    case GD_ECCS_SEE_EXT:
        return s_gd_eccse_4bit_strength_map[GD_ECC_FIELD(raw_status_f0)];
    case GD_ECCS_UNCORRECTABLE:
        return NAND_ECC_NOT_CORRECTED;
    case GD_ECCS_11B:
        /* Reserved/invalid for the 4-bit-strength family per datasheet. */
        return NAND_ECC_INVALID;
    }
    return NAND_ECC_INVALID;    /* unreachable: GD_ECC_FIELD() is 2 bits wide */
}
