/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_ecc_decode.h"
#include "nand_gigadevice_ecc_decode.h"

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

TEST_CASE("2-bit ECCS field decodes into nand_ecc_status_t", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_2bit(k_ecc_00, 0) == NAND_ECC_OK);
    REQUIRE(nand_ecc_decode_2bit(k_ecc_01, 0) == NAND_ECC_1_TO_3_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit(k_ecc_10, 0) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit(k_ecc_11, 0) == NAND_ECC_4_TO_6_BITS_CORRECTED);
}

TEST_CASE("3-bit ECCS field decodes into nand_ecc_status_t", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_3bit(k_ecc_00, 0) == NAND_ECC_OK);
    REQUIRE(nand_ecc_decode_3bit(k_ecc_01, 0) == NAND_ECC_1_TO_3_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_3bit(k_ecc_10, 0) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(nand_ecc_decode_3bit(k_ecc_11, 0) == NAND_ECC_4_TO_6_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_3bit(k_ecc_101, 0) == NAND_ECC_7_8_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_3bit(k_ecc_100, 0) == NAND_ECC_INVALID);
    REQUIRE(nand_ecc_decode_3bit(k_ecc_110, 0) == NAND_ECC_INVALID);
    REQUIRE(nand_ecc_decode_3bit(k_ecc_111, 0) == NAND_ECC_INVALID);
}

TEST_CASE("2-bit ECCS, strength 1: 01 is exactly 1, 10 and 11 are uncorrectable", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_2bit_strength_1(k_ecc_00, 0) == NAND_ECC_OK);
    REQUIRE(nand_ecc_decode_2bit_strength_1(k_ecc_01, 0) == NAND_ECC_1_BIT_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_strength_1(k_ecc_10, 0) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_strength_1(k_ecc_11, 0) == NAND_ECC_NOT_CORRECTED);
    /* Bit 6 is outside the 2-bit field and must be ignored. */
    REQUIRE(nand_ecc_decode_2bit_strength_1(k_ecc_101, 0) == NAND_ECC_1_BIT_CORRECTED);
}

TEST_CASE("2-bit ECCS, strength 4: 01 is 1-3 corrected, 11 is exactly 4", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_2bit_strength_4(k_ecc_00, 0) == NAND_ECC_OK);
    REQUIRE(nand_ecc_decode_2bit_strength_4(k_ecc_01, 0) == NAND_ECC_1_TO_3_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_strength_4(k_ecc_10, 0) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_strength_4(k_ecc_11, 0) == NAND_ECC_4_BITS_CORRECTED);
    /* Bit 6 is outside the 2-bit field and must be ignored. */
    REQUIRE(nand_ecc_decode_2bit_strength_4(k_ecc_111, 0) == NAND_ECC_4_BITS_CORRECTED);
}

TEST_CASE("2-bit ECCS, strength 8: 01 is 1-7 corrected, 11 is exactly 8", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_2bit_strength_8(k_ecc_00, 0) == NAND_ECC_OK);
    REQUIRE(nand_ecc_decode_2bit_strength_8(k_ecc_01, 0) == NAND_ECC_1_TO_7_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_strength_8(k_ecc_10, 0) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_strength_8(k_ecc_11, 0) == NAND_ECC_8_BITS_CORRECTED);
    /* Bit 6 is outside the 2-bit field and must be ignored. */
    REQUIRE(nand_ecc_decode_2bit_strength_8(k_ecc_101, 0) == NAND_ECC_1_TO_7_BITS_CORRECTED);
}

TEST_CASE("XTX ECCS3:0 decodes all 16 patterns", "[spi_nand_flash][ecc]")
{
    /* ECCS1:0 = 01b: ECCS3:2 gives the count */
    REQUIRE(nand_ecc_decode_xtx(0b0001'0000, 0) == NAND_ECC_1_TO_4_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_xtx(0b0101'0000, 0) == NAND_ECC_5_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_xtx(0b1001'0000, 0) == NAND_ECC_6_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_xtx(0b1101'0000, 0) == NAND_ECC_7_BITS_CORRECTED);
    /* ECCS1:0 = 00b / 10b / 11b: ECCS3:2 is don't-care */
    for (uint8_t hi = 0; hi < 4; hi++) {
        const uint8_t h = (uint8_t)(hi << 6);
        REQUIRE(nand_ecc_decode_xtx(h | k_ecc_00, 0) == NAND_ECC_OK);
        REQUIRE(nand_ecc_decode_xtx(h | k_ecc_10, 0) == NAND_ECC_NOT_CORRECTED);
        REQUIRE(nand_ecc_decode_xtx(h | k_ecc_11, 0) == NAND_ECC_8_BITS_CORRECTED);
    }
    /* Bits [3:0] are not ECC status and must be ignored. */
    REQUIRE(nand_ecc_decode_xtx(0b0101'1111, 0) == NAND_ECC_5_BITS_CORRECTED);
}

TEST_CASE("2-bit ECCS with count: count is needed only for 01b or 11b", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_2bit_reports_correction(k_ecc_00) == false);
    REQUIRE(nand_ecc_2bit_reports_correction(k_ecc_01) == true);
    REQUIRE(nand_ecc_2bit_reports_correction(k_ecc_10) == false);
    REQUIRE(nand_ecc_2bit_reports_correction(k_ecc_11) == true);
    /* Bit 6 is outside the 2-bit field and must be ignored. */
    REQUIRE(nand_ecc_2bit_reports_correction(k_ecc_100) == false);
}

TEST_CASE("2-bit ECCS with count: 00/10 ignore a stale count", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_2bit_with_count(k_ecc_00, 5) == NAND_ECC_OK);
    REQUIRE(nand_ecc_decode_2bit_with_count(k_ecc_10, 5) == NAND_ECC_NOT_CORRECTED);
}

TEST_CASE("2-bit ECCS with count: 01/11 trust the count", "[spi_nand_flash][ecc]")
{
    static const nand_ecc_status_t expected[9] = {
        NAND_ECC_OK, NAND_ECC_1_BIT_CORRECTED, NAND_ECC_2_BITS_CORRECTED,
        NAND_ECC_3_BITS_CORRECTED, NAND_ECC_4_BITS_CORRECTED, NAND_ECC_5_BITS_CORRECTED,
        NAND_ECC_6_BITS_CORRECTED, NAND_ECC_7_BITS_CORRECTED, NAND_ECC_8_BITS_CORRECTED,
    };
    for (uint8_t n = 0; n <= 8; n++) {
        REQUIRE(nand_ecc_decode_2bit_with_count(k_ecc_01, n) == expected[n]);
        REQUIRE(nand_ecc_decode_2bit_with_count(k_ecc_11, n) == expected[n]);
    }
    /* 9-14 undefined */
    for (uint8_t n = 9; n <= 14; n++) {
        REQUIRE(nand_ecc_decode_2bit_with_count(k_ecc_01, n) == NAND_ECC_INVALID);
        REQUIRE(nand_ecc_decode_2bit_with_count(k_ecc_11, n) == NAND_ECC_INVALID);
    }
    /* 1111b is >8 errors: uncorrectable, even though ECCS said corrected */
    REQUIRE(nand_ecc_decode_2bit_with_count(k_ecc_01, 0x0F) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(nand_ecc_decode_2bit_with_count(k_ecc_11, 0x0F) == NAND_ECC_NOT_CORRECTED);
}

TEST_CASE("GD reads F0h only when ECCS is 01b", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_gd_ecc_needs_status_ext(k_ecc_01) == true);
    REQUIRE(nand_gd_ecc_needs_status_ext(k_ecc_00) == false);
    REQUIRE(nand_gd_ecc_needs_status_ext(k_ecc_10) == false);
    REQUIRE(nand_gd_ecc_needs_status_ext(k_ecc_11) == false);
}

TEST_CASE("GD ECCS 00/10 ignore stale F0h", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_gd_ecc_decode_8bit_strength(k_ecc_00, k_ecc_11) == NAND_ECC_OK);
    REQUIRE(nand_gd_ecc_decode_8bit_strength(k_ecc_10, k_ecc_11) == NAND_ECC_NOT_CORRECTED);
}

TEST_CASE("GD 8-bit-strength ECCS 11 maps to exactly 8 without using F0h", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_gd_ecc_decode_8bit_strength(k_ecc_11, k_ecc_00) == NAND_ECC_8_BITS_CORRECTED);
    REQUIRE(nand_gd_ecc_decode_8bit_strength(k_ecc_11, k_ecc_11) == NAND_ECC_8_BITS_CORRECTED);
}

TEST_CASE("GD 8-bit-strength ECCS 01 uses exact ECCSE counts", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_gd_ecc_decode_8bit_strength(k_ecc_01, k_ecc_00) == NAND_ECC_1_TO_4_BITS_CORRECTED);
    REQUIRE(nand_gd_ecc_decode_8bit_strength(k_ecc_01, k_ecc_01) == NAND_ECC_5_BITS_CORRECTED);
    REQUIRE(nand_gd_ecc_decode_8bit_strength(k_ecc_01, k_ecc_10) == NAND_ECC_6_BITS_CORRECTED);
    REQUIRE(nand_gd_ecc_decode_8bit_strength(k_ecc_01, k_ecc_11) == NAND_ECC_7_BITS_CORRECTED);
}

TEST_CASE("GD 4-bit-strength ECCS 00/10 ignore stale F0h", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_gd_ecc_decode_4bit_strength(k_ecc_00, k_ecc_11) == NAND_ECC_OK);
    REQUIRE(nand_gd_ecc_decode_4bit_strength(k_ecc_10, k_ecc_11) == NAND_ECC_NOT_CORRECTED);
}

TEST_CASE("GD 4-bit-strength ECCS 11 is reserved/invalid", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_gd_ecc_decode_4bit_strength(k_ecc_11, k_ecc_00) == NAND_ECC_INVALID);
    REQUIRE(nand_gd_ecc_decode_4bit_strength(k_ecc_11, k_ecc_11) == NAND_ECC_INVALID);
}

TEST_CASE("GD 4-bit-strength ECCS 01 uses exact ECCSE counts", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_gd_ecc_decode_4bit_strength(k_ecc_01, k_ecc_00) == NAND_ECC_1_BIT_CORRECTED);
    REQUIRE(nand_gd_ecc_decode_4bit_strength(k_ecc_01, k_ecc_01) == NAND_ECC_2_BITS_CORRECTED);
    REQUIRE(nand_gd_ecc_decode_4bit_strength(k_ecc_01, k_ecc_10) == NAND_ECC_3_BITS_CORRECTED);
    REQUIRE(nand_gd_ecc_decode_4bit_strength(k_ecc_01, k_ecc_11) == NAND_ECC_4_BITS_CORRECTED);
}

TEST_CASE("refresh threshold uses max bits per range, safe for a tunable threshold", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_OK) == 0);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_1_TO_3_BITS_CORRECTED) == 3);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_NOT_CORRECTED) == NAND_ECC_BITS_UNKNOWN);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_1_TO_4_BITS_CORRECTED) == 4);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_4_TO_6_BITS_CORRECTED) == 6);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_5_BITS_CORRECTED) == 5);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_6_BITS_CORRECTED) == 6);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_7_BITS_CORRECTED) == 7);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_7_8_BITS_CORRECTED) == 8);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_8_BITS_CORRECTED) == 8);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_1_BIT_CORRECTED) == 1);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_2_BITS_CORRECTED) == 2);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_3_BITS_CORRECTED) == 3);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_4_BITS_CORRECTED) == 4);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_1_TO_7_BITS_CORRECTED) == 7);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_INVALID) == NAND_ECC_BITS_UNKNOWN);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_MAX) == NAND_ECC_BITS_UNKNOWN);
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
}
