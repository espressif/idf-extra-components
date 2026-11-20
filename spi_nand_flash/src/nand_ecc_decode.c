/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include "nand_ecc_decode.h"
#include "nand.h"

static const nand_ecc_status_t s_ecc_2bit_default_map[4] = {
    [0] = NAND_ECC_OK,
    [1] = NAND_ECC_1_TO_3_BITS_CORRECTED,
    [2] = NAND_ECC_NOT_CORRECTED,
    [3] = NAND_ECC_4_TO_6_BITS_CORRECTED,
};

/* Raw 3-bit ECCS -> driver status. Reserved combinations (4, 6, 7) are invalid. */
static const nand_ecc_status_t s_ecc_3bit_map[8] = {
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
static const nand_ecc_status_t s_ecc_2bit_max4_map[4] = {
    [0] = NAND_ECC_OK,
    [1] = NAND_ECC_1_TO_3_BITS_CORRECTED,
    [2] = NAND_ECC_NOT_CORRECTED,
    [3] = NAND_ECC_4_BITS_CORRECTED,
};

static const nand_ecc_status_t s_ecc_2bit_max8_map[4] = {
    [0] = NAND_ECC_OK,
    [1] = NAND_ECC_1_TO_7_BITS_CORRECTED,
    [2] = NAND_ECC_NOT_CORRECTED,
    [3] = NAND_ECC_8_BITS_CORRECTED,
};

/* 1-bit (Hamming) ECC: 01b is exactly 1 bit corrected. 10b is a 2-bit error in the page;
 * 11b is 2-bit errors in multiple pages (continuous read only). Both are uncorrectable. */
static const nand_ecc_status_t s_ecc_2bit_max1_map[4] = {
    [0] = NAND_ECC_OK,
    [1] = NAND_ECC_1_BIT_CORRECTED,
    [2] = NAND_ECC_NOT_CORRECTED,
    [3] = NAND_ECC_NOT_CORRECTED,
};

/* 4-bit vendor error count field -> status: 0-8 exact count, 1111b ">8" (uncorrectable),
 * 1001b-1110b undefined. */
#define ECC_COUNT_OVER_MAX  0b1111u

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

nand_ecc_status_t nand_ecc_decode_2bit_default(uint8_t status_c0)
{
    return s_ecc_2bit_default_map[NAND_ECC_2BIT_FIELD(status_c0)];
}

nand_ecc_status_t nand_ecc_decode_3bit(uint8_t status_c0)
{
    return s_ecc_3bit_map[NAND_ECC_3BIT_FIELD(status_c0)];
}

nand_ecc_status_t nand_ecc_decode_unknown(uint8_t status_c0)
{
    (void)status_c0;
    return NAND_ECC_UNKNOWN;
}

nand_ecc_status_t nand_ecc_decode_2bit_max4(uint8_t status_c0)
{
    return s_ecc_2bit_max4_map[NAND_ECC_2BIT_FIELD(status_c0)];
}

nand_ecc_status_t nand_ecc_decode_2bit_max8(uint8_t status_c0)
{
    return s_ecc_2bit_max8_map[NAND_ECC_2BIT_FIELD(status_c0)];
}

nand_ecc_status_t nand_ecc_decode_2bit_max1(uint8_t status_c0)
{
    return s_ecc_2bit_max1_map[NAND_ECC_2BIT_FIELD(status_c0)];
}

const nand_ecc_decoder_t nand_ecc_decoder_2bit_default = { .decode = nand_ecc_decode_2bit_default };
const nand_ecc_decoder_t nand_ecc_decoder_3bit         = { .decode = nand_ecc_decode_3bit };
const nand_ecc_decoder_t nand_ecc_decoder_2bit_max1    = { .decode = nand_ecc_decode_2bit_max1 };
const nand_ecc_decoder_t nand_ecc_decoder_2bit_max4    = { .decode = nand_ecc_decode_2bit_max4 };
const nand_ecc_decoder_t nand_ecc_decoder_2bit_max8    = { .decode = nand_ecc_decode_2bit_max8 };
const nand_ecc_decoder_t nand_ecc_decoder_unknown      = { .decode = nand_ecc_decode_unknown };

nand_ecc_status_t nand_ecc_status_from_count(uint8_t count)
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

esp_err_t nand_ecc_check_status(spi_nand_flash_device_t *dev, uint8_t status_c0)
{
    const nand_ecc_decoder_t *decoder = dev->ecc_decoder;
    nand_ecc_status_t *out = &dev->chip.ecc_data.ecc_corrected_bits_status;
    assert(decoder && decoder->decode);
    const nand_ecc_ext_reg_ops_t *ext_reg_ops = decoder->ext_reg_ops;
    nand_ecc_status_t status;
    if (ext_reg_ops && ext_reg_ops->needs_read(status_c0)) {
        uint8_t ext_reg_value;
        esp_err_t ret = ext_reg_ops->read(dev, &ext_reg_value);
        if (ret != ESP_OK) {
            /* Status unknown: not an ECC verdict, so don't report NOT_CORRECTED. */
            *out = NAND_ECC_INVALID;
            return ret;
        }
        status = ext_reg_ops->decode(ext_reg_value);
    } else {
        status = decoder->decode(status_c0);
    }
    *out = status;
    /* NAND_ECC_MAX is not a status; a decoder returning it is a bug, so fail safe. */
    if (status == NAND_ECC_NOT_CORRECTED || status == NAND_ECC_INVALID || status >= NAND_ECC_MAX) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
