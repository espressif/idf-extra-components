/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand.h"
#include "nand_ecc_decode.h"
#include "nand_gigadevice_ecc_decode.h"
#include "nand_xtx_ecc_decode.h"

#include <catch2/catch_test_macros.hpp>

/* C0h / F0h ECC fields occupy bits [5:4] (and [6] for 3-bit). */
static const uint8_t k_ecc_00  = 0b0000'0000;
static const uint8_t k_ecc_01  = 0b0001'0000;
static const uint8_t k_ecc_10  = 0b0010'0000;
static const uint8_t k_ecc_11  = 0b0011'0000;
static const uint8_t k_ecc_100 = 0b0100'0000; /* reserved 3-bit */
static const uint8_t k_ecc_101 = 0b0101'0000; /* Micron 7-8 */
static const uint8_t k_ecc_110 = 0b0110'0000; /* reserved 3-bit */
static const uint8_t k_ecc_111 = 0b0111'0000; /* reserved 3-bit */

TEST_CASE("2bit default decodes into nand_ecc_status_t", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_2bit_default(k_ecc_00) == NAND_ECC_OK);
    REQUIRE(nand_ecc_decode_2bit_default(k_ecc_01) == NAND_ECC_1_TO_3_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_default(k_ecc_10) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_default(k_ecc_11) == NAND_ECC_4_TO_6_BITS_CORRECTED);
}

TEST_CASE("unknown decoder ignores C0h and reports NAND_ECC_UNKNOWN", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_unknown(k_ecc_10) == NAND_ECC_UNKNOWN);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_UNKNOWN) == NAND_ECC_BITS_UNKNOWN);
}

TEST_CASE("3bit decodes into nand_ecc_status_t", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_3bit(k_ecc_00) == NAND_ECC_OK);
    REQUIRE(nand_ecc_decode_3bit(k_ecc_01) == NAND_ECC_1_TO_3_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_3bit(k_ecc_10) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(nand_ecc_decode_3bit(k_ecc_11) == NAND_ECC_4_TO_6_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_3bit(k_ecc_101) == NAND_ECC_7_8_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_3bit(k_ecc_100) == NAND_ECC_INVALID);
    REQUIRE(nand_ecc_decode_3bit(k_ecc_110) == NAND_ECC_INVALID);
    REQUIRE(nand_ecc_decode_3bit(k_ecc_111) == NAND_ECC_INVALID);
}

TEST_CASE("2bit t1: 01 is exactly 1, 10 and 11 are uncorrectable", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_2bit_max1(k_ecc_00) == NAND_ECC_OK);
    REQUIRE(nand_ecc_decode_2bit_max1(k_ecc_01) == NAND_ECC_1_BIT_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_max1(k_ecc_10) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_max1(k_ecc_11) == NAND_ECC_NOT_CORRECTED);
}

TEST_CASE("2bit t4: 01 is 1-3 corrected, 11 is exactly 4", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_2bit_max4(k_ecc_00) == NAND_ECC_OK);
    REQUIRE(nand_ecc_decode_2bit_max4(k_ecc_01) == NAND_ECC_1_TO_3_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_max4(k_ecc_10) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_max4(k_ecc_11) == NAND_ECC_4_BITS_CORRECTED);
}

TEST_CASE("2bit t8: 01 is 1-7 corrected, 11 is exactly 8", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_2bit_max8(k_ecc_00) == NAND_ECC_OK);
    REQUIRE(nand_ecc_decode_2bit_max8(k_ecc_01) == NAND_ECC_1_TO_7_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_max8(k_ecc_10) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_max8(k_ecc_11) == NAND_ECC_8_BITS_CORRECTED);
}

TEST_CASE("XTX ECCS3:0 decodes all 16 patterns", "[spi_nand_flash][ecc]")
{
    /* ECCS1:0 = 01b: ECCS3:2 gives the count */
    REQUIRE(nand_xtx_ecc_decode(0b0001'0000) == NAND_ECC_1_TO_4_BITS_CORRECTED);
    REQUIRE(nand_xtx_ecc_decode(0b0101'0000) == NAND_ECC_5_BITS_CORRECTED);
    REQUIRE(nand_xtx_ecc_decode(0b1001'0000) == NAND_ECC_6_BITS_CORRECTED);
    REQUIRE(nand_xtx_ecc_decode(0b1101'0000) == NAND_ECC_7_BITS_CORRECTED);
    /* ECCS1:0 = 00b / 10b / 11b: ECCS3:2 is don't-care */
    for (uint8_t hi = 0; hi < 4; hi++) {
        const uint8_t h = (uint8_t)(hi << 6);
        REQUIRE(nand_xtx_ecc_decode(h | k_ecc_00) == NAND_ECC_OK);
        REQUIRE(nand_xtx_ecc_decode(h | k_ecc_10) == NAND_ECC_NOT_CORRECTED);
        REQUIRE(nand_xtx_ecc_decode(h | k_ecc_11) == NAND_ECC_8_BITS_CORRECTED);
    }
}

TEST_CASE("2bit: vendor count register is needed only for 01b or 11b", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_2bit_reports_correction(k_ecc_00) == false);
    REQUIRE(nand_ecc_2bit_reports_correction(k_ecc_01) == true);
    REQUIRE(nand_ecc_2bit_reports_correction(k_ecc_10) == false);
    REQUIRE(nand_ecc_2bit_reports_correction(k_ecc_11) == true);
}

TEST_CASE("count from vendor register maps to nand_ecc_status_t", "[spi_nand_flash][ecc]")
{
    static const nand_ecc_status_t expected[9] = {
        NAND_ECC_OK, NAND_ECC_1_BIT_CORRECTED, NAND_ECC_2_BITS_CORRECTED,
        NAND_ECC_3_BITS_CORRECTED, NAND_ECC_4_BITS_CORRECTED, NAND_ECC_5_BITS_CORRECTED,
        NAND_ECC_6_BITS_CORRECTED, NAND_ECC_7_BITS_CORRECTED, NAND_ECC_8_BITS_CORRECTED,
    };
    for (uint8_t n = 0; n <= 8; n++) {
        REQUIRE(nand_ecc_status_from_count(n) == expected[n]);
    }
    /* 9-14 undefined */
    for (uint8_t n = 9; n <= 14; n++) {
        REQUIRE(nand_ecc_status_from_count(n) == NAND_ECC_INVALID);
    }
    /* 1111b is >8 errors: uncorrectable, even though ECCS said corrected */
    REQUIRE(nand_ecc_status_from_count(0x0F) == NAND_ECC_NOT_CORRECTED);
}

TEST_CASE("GD reads F0h only when ECCS is 01b", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_gd_ecc_needs_status_ext(k_ecc_01) == true);
    REQUIRE(nand_gd_ecc_needs_status_ext(k_ecc_00) == false);
    REQUIRE(nand_gd_ecc_needs_status_ext(k_ecc_10) == false);
    REQUIRE(nand_gd_ecc_needs_status_ext(k_ecc_11) == false);
}

TEST_CASE("GD t4 C0h: 11 is reserved/invalid", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_gd_ecc_decode_max4(k_ecc_00) == NAND_ECC_OK);
    REQUIRE(nand_gd_ecc_decode_max4(k_ecc_10) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(nand_gd_ecc_decode_max4(k_ecc_11) == NAND_ECC_INVALID);
}

TEST_CASE("GD t8 F0h ECCSE gives 1-4, 5, 6 or 7", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_gd_ecc_decode_eccse_max8(k_ecc_00) == NAND_ECC_1_TO_4_BITS_CORRECTED);
    REQUIRE(nand_gd_ecc_decode_eccse_max8(k_ecc_01) == NAND_ECC_5_BITS_CORRECTED);
    REQUIRE(nand_gd_ecc_decode_eccse_max8(k_ecc_10) == NAND_ECC_6_BITS_CORRECTED);
    REQUIRE(nand_gd_ecc_decode_eccse_max8(k_ecc_11) == NAND_ECC_7_BITS_CORRECTED);
}

TEST_CASE("GD t4 F0h ECCSE gives exactly 1-4", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_gd_ecc_decode_eccse_max4(k_ecc_00) == NAND_ECC_1_BIT_CORRECTED);
    REQUIRE(nand_gd_ecc_decode_eccse_max4(k_ecc_01) == NAND_ECC_2_BITS_CORRECTED);
    REQUIRE(nand_gd_ecc_decode_eccse_max4(k_ecc_10) == NAND_ECC_3_BITS_CORRECTED);
    REQUIRE(nand_gd_ecc_decode_eccse_max4(k_ecc_11) == NAND_ECC_4_BITS_CORRECTED);
}

TEST_CASE("C0h/F0h decoders read only their ECC field bits", "[spi_nand_flash][ecc]")
{
    /* Real status bytes carry OIP/WELL/E_FAIL/P_FAIL in [3:0] and vendor bits above the field. */
    nand_ecc_status_t (*const field_2bit[])(uint8_t) = {
        nand_ecc_decode_2bit_default, nand_ecc_decode_2bit_max1, nand_ecc_decode_2bit_max4,
        nand_ecc_decode_2bit_max8, nand_gd_ecc_decode_max4, nand_gd_ecc_decode_eccse_max4,
        nand_gd_ecc_decode_eccse_max8,
    };
    for (int s = 0; s < 256; s++) {
        const uint8_t reg = (uint8_t)s;
        for (auto decode : field_2bit) {
            REQUIRE(decode(reg) == decode(reg & 0b0011'0000));
        }
        REQUIRE(nand_ecc_decode_3bit(reg) == nand_ecc_decode_3bit(reg & 0b0111'0000));
        REQUIRE(nand_xtx_ecc_decode(reg) == nand_xtx_ecc_decode(reg & 0b1111'0000));
        REQUIRE(nand_ecc_2bit_reports_correction(reg) == nand_ecc_2bit_reports_correction(reg & 0b0011'0000));
        REQUIRE(nand_gd_ecc_needs_status_ext(reg) == nand_gd_ecc_needs_status_ext(reg & 0b0011'0000));
    }
}

TEST_CASE("every correctable status has a refresh ceiling", "[spi_nand_flash][ecc]")
{
    for (int v = 0; v < NAND_ECC_MAX; v++) {
        const nand_ecc_status_t st = (nand_ecc_status_t)v;
        if (v == 4) {
            continue;   /* unused value, kept free for numbering compatibility */
        }
        if (st == NAND_ECC_NOT_CORRECTED || st == NAND_ECC_INVALID || st == NAND_ECC_UNKNOWN) {
            REQUIRE(nand_ecc_max_bits_corrected(st) == NAND_ECC_BITS_UNKNOWN);
        } else {
            REQUIRE(nand_ecc_max_bits_corrected(st) <= 8);
        }
    }
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_MAX) == NAND_ECC_BITS_UNKNOWN);
    /* Ranges report their ceiling, so a tunable threshold never under-refreshes. */
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_1_TO_3_BITS_CORRECTED) == 3);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_1_TO_4_BITS_CORRECTED) == 4);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_4_TO_6_BITS_CORRECTED) == 6);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_1_TO_7_BITS_CORRECTED) == 7);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_7_8_BITS_CORRECTED) == 8);
    for (uint8_t n = 0; n <= 8; n++) {
        REQUIRE(nand_ecc_max_bits_corrected(nand_ecc_status_from_count(n)) == n);
    }
}

static bool exceeds(nand_ecc_status_t st, uint8_t threshold)
{
    nand_ecc_data_t ecc = {};
    ecc.ecc_corrected_bits_status = st;
    ecc.ecc_data_refresh_threshold = threshold;
    return nand_ecc_exceeds_data_refresh_threshold(&ecc);
}

TEST_CASE("refresh threshold decision", "[spi_nand_flash][ecc]")
{
    REQUIRE_FALSE(exceeds(NAND_ECC_OK, 1));
    REQUIRE(exceeds(NAND_ECC_4_BITS_CORRECTED, 4));
    REQUIRE_FALSE(exceeds(NAND_ECC_3_BITS_CORRECTED, 4));
    REQUIRE(exceeds(NAND_ECC_1_TO_3_BITS_CORRECTED, 3));
    REQUIRE_FALSE(exceeds(NAND_ECC_NOT_CORRECTED, 1));
    REQUIRE_FALSE(exceeds(NAND_ECC_INVALID, 1));
    REQUIRE_FALSE(exceeds(NAND_ECC_UNKNOWN, 1));
    /* Winbond 1-bit parts use threshold 1: any correction refreshes. */
    REQUIRE(exceeds(NAND_ECC_1_BIT_CORRECTED, 1));
    /* 8-bit-strength chips without a count register: any 01b correction refreshes at the default 4. */
    REQUIRE(exceeds(NAND_ECC_1_TO_7_BITS_CORRECTED, 4));
}

/* Fake extension register for nand_ecc_check_status(): read() returns s_ext_value / s_ext_ret. */
static int s_ext_reads;
static uint8_t s_ext_value;
static esp_err_t s_ext_ret;

static esp_err_t fake_ext_read(spi_nand_flash_device_t *dev, uint8_t *reg_value)
{
    (void)dev;
    s_ext_reads++;
    *reg_value = s_ext_value;
    return s_ext_ret;
}

static const nand_ecc_ext_reg_ops_t s_fake_ext_reg_ops = {
    nand_ecc_2bit_reports_correction, fake_ext_read, nand_ecc_status_from_count,
};
static const nand_ecc_decoder_t s_fake_decoder = { nand_ecc_decode_2bit_default, &s_fake_ext_reg_ops };

static esp_err_t check(const nand_ecc_decoder_t *decoder, uint8_t c0, uint8_t ext_value, esp_err_t ext_ret,
                       nand_ecc_status_t *out)
{
    s_ext_reads = 0;
    s_ext_value = ext_value;
    s_ext_ret = ext_ret;
    spi_nand_flash_device_t dev = {};
    dev.ecc_decoder = decoder;
    dev.chip.ecc_data.ecc_corrected_bits_status = NAND_ECC_MAX;
    esp_err_t ret = nand_ecc_check_status(&dev, c0);
    *out = dev.chip.ecc_data.ecc_corrected_bits_status;
    return ret;
}

TEST_CASE("check_status: C0h-only decoder maps status to the read verdict", "[spi_nand_flash][ecc]")
{
    nand_ecc_status_t st;
    REQUIRE(check(&nand_ecc_decoder_2bit_default, k_ecc_00, 0, ESP_OK, &st) == ESP_OK);
    REQUIRE(st == NAND_ECC_OK);
    REQUIRE(check(&nand_ecc_decoder_2bit_default, k_ecc_11, 0, ESP_OK, &st) == ESP_OK);
    REQUIRE(st == NAND_ECC_4_TO_6_BITS_CORRECTED);
    REQUIRE(check(&nand_ecc_decoder_2bit_default, k_ecc_10, 0, ESP_OK, &st) == ESP_FAIL);
    REQUIRE(st == NAND_ECC_NOT_CORRECTED);
    REQUIRE(check(&nand_ecc_decoder_3bit, k_ecc_110, 0, ESP_OK, &st) == ESP_FAIL);
    REQUIRE(st == NAND_ECC_INVALID);
    /* Generic chip path: no verdict, so the read is not failed. */
    REQUIRE(check(&nand_ecc_decoder_unknown, k_ecc_10, 0, ESP_OK, &st) == ESP_OK);
    REQUIRE(st == NAND_ECC_UNKNOWN);
}

TEST_CASE("check_status: extension register is read only when needs_read is true", "[spi_nand_flash][ecc]")
{
    nand_ecc_status_t st;
    REQUIRE(check(&s_fake_decoder, k_ecc_00, 5, ESP_OK, &st) == ESP_OK);
    REQUIRE(s_ext_reads == 0);
    REQUIRE(st == NAND_ECC_OK);
    REQUIRE(check(&s_fake_decoder, k_ecc_10, 5, ESP_OK, &st) == ESP_FAIL);
    REQUIRE(s_ext_reads == 0);
    REQUIRE(st == NAND_ECC_NOT_CORRECTED);

    /* 01b and 11b: the register value wins over the C0h table (1-3 / 4-6). */
    REQUIRE(check(&s_fake_decoder, k_ecc_01, 5, ESP_OK, &st) == ESP_OK);
    REQUIRE(s_ext_reads == 1);
    REQUIRE(st == NAND_ECC_5_BITS_CORRECTED);
    REQUIRE(check(&s_fake_decoder, k_ecc_11, 2, ESP_OK, &st) == ESP_OK);
    REQUIRE(s_ext_reads == 1);
    REQUIRE(st == NAND_ECC_2_BITS_CORRECTED);
}

TEST_CASE("check_status: extension register value can fail the read", "[spi_nand_flash][ecc]")
{
    nand_ecc_status_t st;
    REQUIRE(check(&s_fake_decoder, k_ecc_01, 0b1111, ESP_OK, &st) == ESP_FAIL);
    REQUIRE(st == NAND_ECC_NOT_CORRECTED);
    REQUIRE(check(&s_fake_decoder, k_ecc_01, 9, ESP_OK, &st) == ESP_FAIL);
    REQUIRE(st == NAND_ECC_INVALID);
}

TEST_CASE("check_status: failed register read is passed through, not an ECC verdict", "[spi_nand_flash][ecc]")
{
    nand_ecc_status_t st;
    REQUIRE(check(&s_fake_decoder, k_ecc_01, 5, ESP_ERR_TIMEOUT, &st) == ESP_ERR_TIMEOUT);
    REQUIRE(s_ext_reads == 1);
    REQUIRE(st == NAND_ECC_INVALID);
}
