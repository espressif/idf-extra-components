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

static nand_ecc_status_t decode_2bit(uint8_t c0)
{
    nand_ecc_status_t st = NAND_ECC_OK;
    REQUIRE(nand_ecc_decode_2bit(NULL, c0, &st) == ESP_OK);
    return st;
}

static nand_ecc_status_t decode_3bit(uint8_t c0)
{
    nand_ecc_status_t st = NAND_ECC_OK;
    REQUIRE(nand_ecc_decode_3bit(NULL, c0, &st) == ESP_OK);
    return st;
}

TEST_CASE("2-bit ECCS field decodes into nand_ecc_status_t", "[spi_nand_flash][ecc]")
{
    REQUIRE(decode_2bit(k_ecc_00) == NAND_ECC_OK);
    REQUIRE(decode_2bit(k_ecc_01) == NAND_ECC_1_TO_3_BITS_CORRECTED);
    REQUIRE(decode_2bit(k_ecc_10) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(decode_2bit(k_ecc_11) == NAND_ECC_4_TO_6_BITS_CORRECTED);
}

TEST_CASE("3-bit ECCS field decodes into nand_ecc_status_t", "[spi_nand_flash][ecc]")
{
    REQUIRE(decode_3bit(k_ecc_00) == NAND_ECC_OK);
    REQUIRE(decode_3bit(k_ecc_01) == NAND_ECC_1_TO_3_BITS_CORRECTED);
    REQUIRE(decode_3bit(k_ecc_10) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(decode_3bit(k_ecc_11) == NAND_ECC_4_TO_6_BITS_CORRECTED);
    REQUIRE(decode_3bit(k_ecc_101) == NAND_ECC_7_8_BITS_CORRECTED);
    REQUIRE(decode_3bit(k_ecc_100) == NAND_ECC_INVALID);
    REQUIRE(decode_3bit(k_ecc_110) == NAND_ECC_INVALID);
    REQUIRE(decode_3bit(k_ecc_111) == NAND_ECC_INVALID);
}

static nand_ecc_status_t decode_2bit_8bit_strength(uint8_t c0)
{
    nand_ecc_status_t st = NAND_ECC_MAX;
    REQUIRE(nand_ecc_decode_2bit_8bit_strength(NULL, c0, &st) == ESP_OK);
    return st;
}

TEST_CASE("2-bit ECCS, 8-bit strength: 01 is 1-7 corrected, 11 is exactly 8", "[spi_nand_flash][ecc]")
{
    REQUIRE(decode_2bit_8bit_strength(k_ecc_00) == NAND_ECC_OK);
    REQUIRE(decode_2bit_8bit_strength(k_ecc_01) == NAND_ECC_1_TO_7_BITS_CORRECTED);
    REQUIRE(decode_2bit_8bit_strength(k_ecc_10) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(decode_2bit_8bit_strength(k_ecc_11) == NAND_ECC_8_BITS_CORRECTED);
    /* Bit 6 is outside the 2-bit field and must be ignored. */
    REQUIRE(decode_2bit_8bit_strength(k_ecc_101) == NAND_ECC_1_TO_7_BITS_CORRECTED);
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
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_NOT_CORRECTED) == 0);
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
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_INVALID) == 0);
    REQUIRE(nand_ecc_max_bits_corrected(NAND_ECC_MAX) == 0);
}
