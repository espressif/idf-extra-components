/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_ecc_decode.h"

/* 3-bit ECCS field: C0h bits [6:4]. The 2-bit field uses NAND_ECC_2BIT_FIELD(). */
#define NAND_ECC_3BIT_FIELD(reg)  (((reg) >> 4) & 0x7u)

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
static const nand_ecc_status_t s_ecc_2bit_strength_4_map[4] = {
    [0] = NAND_ECC_OK,
    [1] = NAND_ECC_1_TO_3_BITS_CORRECTED,
    [2] = NAND_ECC_NOT_CORRECTED,
    [3] = NAND_ECC_4_BITS_CORRECTED,
};

static const nand_ecc_status_t s_ecc_2bit_strength_8_map[4] = {
    [0] = NAND_ECC_OK,
    [1] = NAND_ECC_1_TO_7_BITS_CORRECTED,
    [2] = NAND_ECC_NOT_CORRECTED,
    [3] = NAND_ECC_8_BITS_CORRECTED,
};

/* 1-bit (Hamming) ECC: 01b is exactly 1 bit corrected. 10b is a 2-bit error in the page;
 * 11b is 2-bit errors in multiple pages (continuous read only). Both are uncorrectable. */
static const nand_ecc_status_t s_ecc_2bit_strength_1_map[4] = {
    [0] = NAND_ECC_OK,
    [1] = NAND_ECC_1_BIT_CORRECTED,
    [2] = NAND_ECC_NOT_CORRECTED,
    [3] = NAND_ECC_NOT_CORRECTED,
};

/* XTX ECCS3:0, indexed by ECCS3:2 (bits [7:6]) when ECCS1:0 (bits [5:4]) is 01b. */
#define XTX_ECCS_HI(reg)  (((reg) >> 6) & 0x3u)
#define XTX_ECCS_CORRECTED    0b01    /* corrected, <= 7; ECCS3:2 gives the count */
#define XTX_ECCS_8_CORRECTED  0b11

static const nand_ecc_status_t s_ecc_xtx_corrected_map[4] = {
    [0] = NAND_ECC_1_TO_4_BITS_CORRECTED,
    [1] = NAND_ECC_5_BITS_CORRECTED,
    [2] = NAND_ECC_6_BITS_CORRECTED,
    [3] = NAND_ECC_7_BITS_CORRECTED,
};

/* 4-bit vendor error count field -> status: 0-8 exact count, 1111b ">8" (uncorrectable),
 * 1001b-1110b undefined. */
#define ECC_COUNT_OVER_MAX  0xFu

static const nand_ecc_status_t s_ecc_count_map[9] = {
    [0] = NAND_ECC_OK,
    [1] = NAND_ECC_1_BIT_CORRECTED,
    [2] = NAND_ECC_2_BITS_CORRECTED,
    [3] = NAND_ECC_3_BITS_CORRECTED,
    [4] = NAND_ECC_4_BITS_CORRECTED,
    [5] = NAND_ECC_5_BITS_CORRECTED,
    [6] = NAND_ECC_6_BITS_CORRECTED,
    [7] = NAND_ECC_7_BITS_CORRECTED,
    [8] = NAND_ECC_8_BITS_CORRECTED,
};

esp_err_t nand_ecc_decode_2bit(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    (void)dev;
    *out = s_ecc_2bit_status_map[NAND_ECC_2BIT_FIELD(status_c0)];
    return ESP_OK;
}

esp_err_t nand_ecc_decode_3bit(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    (void)dev;
    *out = s_ecc_3bit_status_map[NAND_ECC_3BIT_FIELD(status_c0)];
    return ESP_OK;
}

esp_err_t nand_ecc_decode_2bit_strength_4(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    (void)dev;
    *out = s_ecc_2bit_strength_4_map[NAND_ECC_2BIT_FIELD(status_c0)];
    return ESP_OK;
}

esp_err_t nand_ecc_decode_2bit_strength_8(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    (void)dev;
    *out = s_ecc_2bit_strength_8_map[NAND_ECC_2BIT_FIELD(status_c0)];
    return ESP_OK;
}

esp_err_t nand_ecc_decode_2bit_strength_1(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    (void)dev;
    *out = s_ecc_2bit_strength_1_map[NAND_ECC_2BIT_FIELD(status_c0)];
    return ESP_OK;
}

esp_err_t nand_ecc_decode_xtx(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    (void)dev;
    *out = NAND_ECC_INVALID;    /* always overwritten: the switch covers all 4 values */
    switch (NAND_ECC_2BIT_FIELD(status_c0)) {
    case NAND_ECC_2BIT_NO_ERROR:
        *out = NAND_ECC_OK;
        break;
    case XTX_ECCS_CORRECTED:
        *out = s_ecc_xtx_corrected_map[XTX_ECCS_HI(status_c0)];
        break;
    case NAND_ECC_2BIT_UNCORRECTABLE:
        *out = NAND_ECC_NOT_CORRECTED;
        break;
    case XTX_ECCS_8_CORRECTED:
        *out = NAND_ECC_8_BITS_CORRECTED;
        break;
    }
    return ESP_OK;
}

static nand_ecc_status_t ecc_status_from_bit_count(uint8_t count)
{
    if (count < sizeof(s_ecc_count_map) / sizeof(s_ecc_count_map[0])) {
        return s_ecc_count_map[count];
    }
    if (count == ECC_COUNT_OVER_MAX) {
        return NAND_ECC_NOT_CORRECTED;
    }
    return NAND_ECC_INVALID;
}

bool nand_ecc_2bit_reports_correction(uint8_t status_c0)
{
    const uint8_t s = NAND_ECC_2BIT_FIELD(status_c0);
    return s != NAND_ECC_2BIT_NO_ERROR && s != NAND_ECC_2BIT_UNCORRECTABLE;
}

nand_ecc_status_t nand_ecc_decode_2bit_with_count(uint8_t status_c0, uint8_t count)
{
    switch (NAND_ECC_2BIT_FIELD(status_c0)) {
    case NAND_ECC_2BIT_NO_ERROR:
        return NAND_ECC_OK;
    case NAND_ECC_2BIT_UNCORRECTABLE:
        return NAND_ECC_NOT_CORRECTED;
    default:
        /* 01b and 11b only differ by a vendor threshold; the exact count is trusted either way. */
        return ecc_status_from_bit_count(count);
    }
}
