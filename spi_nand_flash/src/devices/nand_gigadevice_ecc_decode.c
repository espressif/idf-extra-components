/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_gigadevice_ecc_decode.h"
#include "nand_ecc_decode.h"

/* C0h ECCS 01b: the count is in F0h ECCSE (also bits [5:4]). */
#define GD_ECCS_SEE_EXT        0b01

/* C0h ECCS for the 4-bit/528B internal-ECC-strength family. 11b is reserved. 01b never
 * reaches this table (F0h is read instead); 1-4 is its meaning without F0h. */
static const nand_ecc_status_t s_gd_eccs_max4_map[4] = {
    [0] = NAND_ECC_OK,
    [1] = NAND_ECC_1_TO_4_BITS_CORRECTED,
    [2] = NAND_ECC_NOT_CORRECTED,
    [3] = NAND_ECC_INVALID,
};

/* F0h ECCSE, valid only when C0h ECCS == 01b.
 * 8-bit/528B internal-ECC-strength family: collapses 1-4 into a ceiling,
 * resolves 5/6/7 exactly; ECCS=11b separately means exactly 8. */
static const nand_ecc_status_t s_gd_eccse_max8_map[4] = {
    [0] = NAND_ECC_1_TO_4_BITS_CORRECTED,
    [1] = NAND_ECC_5_BITS_CORRECTED,
    [2] = NAND_ECC_6_BITS_CORRECTED,
    [3] = NAND_ECC_7_BITS_CORRECTED,
};

/* 4-bit/528B internal-ECC-strength family: ECCSE resolves 1/2/3/4 exactly. */
static const nand_ecc_status_t s_gd_eccse_max4_map[4] = {
    [0] = NAND_ECC_1_BIT_CORRECTED,
    [1] = NAND_ECC_2_BITS_CORRECTED,
    [2] = NAND_ECC_3_BITS_CORRECTED,
    [3] = NAND_ECC_4_BITS_CORRECTED,
};

bool nand_gd_ecc_needs_status_ext(uint8_t status_c0)
{
    return NAND_ECC_2BIT_FIELD(status_c0) == GD_ECCS_SEE_EXT;
}

nand_ecc_status_t nand_gd_ecc_decode_max4(uint8_t status_c0)
{
    return s_gd_eccs_max4_map[NAND_ECC_2BIT_FIELD(status_c0)];
}

/* F0h ECCSE reuses bits [5:4], same positions as C0h ECCS - this is intentional per the
 * datasheet, not a copy-paste of the C0h read. */
nand_ecc_status_t nand_gd_ecc_decode_eccse_max8(uint8_t status_f0)
{
    return s_gd_eccse_max8_map[NAND_ECC_2BIT_FIELD(status_f0)];
}

nand_ecc_status_t nand_gd_ecc_decode_eccse_max4(uint8_t status_f0)
{
    return s_gd_eccse_max4_map[NAND_ECC_2BIT_FIELD(status_f0)];
}
