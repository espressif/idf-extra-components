/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_private/nand_ecc_decode.h"

/* C0h / F0h ECC fields occupy bits [6:4] / [5:4]. Collapse each mask to {0,1}
 * before packing so the index is 0-3 / 0-7, then look up nand_ecc_status_t. */
#define STAT_ECC0  (1u << 4)
#define STAT_ECC1  (1u << 5)
#define STAT_ECC2  (1u << 6)

#define PACK_2BITS_STATUS(status, bit1, bit0)         (((!!((status) & (bit1))) << 1) | \
                                                        (!!((status) & (bit0))))
#define PACK_3BITS_STATUS(status, bit2, bit1, bit0)   (((!!((status) & (bit2))) << 2) | \
                                                       ((!!((status) & (bit1))) << 1) | \
                                                        (!!((status) & (bit0))))

static const nand_ecc_status_t s_ecc_2bit_status_map[4] = {
    [0] = NAND_ECC_OK,
    [1] = NAND_ECC_1_TO_3_BITS_CORRECTED,
    [2] = NAND_ECC_NOT_CORRECTED,
    [3] = NAND_ECC_4_TO_6_BITS_CORRECTED,
};

/* Raw 3-bit ECCS -> driver status. Reserved combinations (4, 6, 7) are invalid. */
static const nand_ecc_status_t s_ecc_3bit_status_map[8] = {
    [0] = NAND_ECC_OK,
    [1] = NAND_ECC_1_TO_3_BITS_CORRECTED,
    [2] = NAND_ECC_NOT_CORRECTED,
    [3] = NAND_ECC_4_TO_6_BITS_CORRECTED,
    [4] = NAND_ECC_MAX,
    [5] = NAND_ECC_7_8_BITS_CORRECTED,
    [6] = NAND_ECC_MAX,
    [7] = NAND_ECC_MAX,
};

/* GD5F4GM8 Table 12-3: F0h ECCSE, valid only when C0h ECCS == 01b. */
static const nand_ecc_status_t s_ecc_gd_eccse_map[4] = {
    [0] = NAND_ECC_1_TO_4_BITS_CORRECTED,
    [1] = NAND_ECC_5_BITS_CORRECTED,
    [2] = NAND_ECC_6_BITS_CORRECTED,
    [3] = NAND_ECC_7_BITS_CORRECTED,
};

nand_ecc_status_t nand_ecc_pack_decode_2bit(uint8_t status_c0)
{
    return s_ecc_2bit_status_map[PACK_2BITS_STATUS(status_c0, STAT_ECC1, STAT_ECC0)];
}

nand_ecc_status_t nand_ecc_pack_decode_3bit(uint8_t status_c0)
{
    return s_ecc_3bit_status_map[PACK_3BITS_STATUS(status_c0, STAT_ECC2, STAT_ECC1, STAT_ECC0)];
}

bool nand_ecc_gd_needs_status_ext(uint8_t status_c0)
{
    return PACK_2BITS_STATUS(status_c0, STAT_ECC1, STAT_ECC0) == 1;
}

nand_ecc_status_t nand_ecc_decode_gd_eccse(uint8_t raw_status_c0, uint8_t raw_status_f0, bool f0_read_ok)
{
    unsigned eccs = PACK_2BITS_STATUS(raw_status_c0, STAT_ECC1, STAT_ECC0);

    if (eccs != 1) {
        if (eccs == 3) {
            return NAND_ECC_8_BITS_CORRECTED;
        }
        return s_ecc_2bit_status_map[eccs];
    }

    if (!f0_read_ok) {
        return NAND_ECC_4_TO_6_BITS_CORRECTED;
    }

    /* F0h ECCSE reuses bits [5:4], same positions as C0h ECCS (GD5F4GM8 Table 12-3) -
     * this is intentional per the datasheet, not a copy-paste of the C0h read. */
    return s_ecc_gd_eccse_map[PACK_2BITS_STATUS(raw_status_f0, STAT_ECC1, STAT_ECC0)];
}

nand_ecc_status_t nand_ecc_decode_2bit(spi_nand_flash_device_t *dev, uint8_t status_c0)
{
    (void)dev;
    return nand_ecc_pack_decode_2bit(status_c0);
}

nand_ecc_status_t nand_ecc_decode_3bit(spi_nand_flash_device_t *dev, uint8_t status_c0)
{
    (void)dev;
    return nand_ecc_pack_decode_3bit(status_c0);
}
