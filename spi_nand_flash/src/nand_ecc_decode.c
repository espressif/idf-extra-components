/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_ecc_decode.h"

/* C0h ECC fields occupy bits [6:4] (3-bit) / [5:4] (2-bit). Collapse each mask to {0,1}
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
    [4] = NAND_ECC_INVALID,
    [5] = NAND_ECC_7_8_BITS_CORRECTED,
    [6] = NAND_ECC_INVALID,
    [7] = NAND_ECC_INVALID,
};

/* 2-bit ECCS where 01b is "corrected, below the maximum" (no count) and 11b is "exactly the
 * maximum corrected". Selected by the chip's internal ECC strength. */
static const nand_ecc_status_t s_ecc_2bit_8bit_strength_map[4] = {
    [0] = NAND_ECC_OK,
    [1] = NAND_ECC_1_TO_7_BITS_CORRECTED,
    [2] = NAND_ECC_NOT_CORRECTED,
    [3] = NAND_ECC_8_BITS_CORRECTED,
};

esp_err_t nand_ecc_decode_2bit(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    (void)dev;
    *out = s_ecc_2bit_status_map[PACK_2BITS_STATUS(status_c0, STAT_ECC1, STAT_ECC0)];
    return ESP_OK;
}

esp_err_t nand_ecc_decode_3bit(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    (void)dev;
    *out = s_ecc_3bit_status_map[PACK_3BITS_STATUS(status_c0, STAT_ECC2, STAT_ECC1, STAT_ECC0)];
    return ESP_OK;
}

esp_err_t nand_ecc_decode_2bit_8bit_strength(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    (void)dev;
    *out = s_ecc_2bit_8bit_strength_map[PACK_2BITS_STATUS(status_c0, STAT_ECC1, STAT_ECC0)];
    return ESP_OK;
}
