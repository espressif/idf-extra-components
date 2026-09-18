/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_private/nand_ecc_decode.h"

#include <catch2/catch_test_macros.hpp>

/* C0h / F0h ECC fields occupy bits [5:4] (and [6] for 3-bit). */
static const uint8_t k_ecc_00 = 0x00;
static const uint8_t k_ecc_01 = 0x10;
static const uint8_t k_ecc_10 = 0x20;
static const uint8_t k_ecc_11 = 0x30;
static const uint8_t k_ecc_100 = 0x40; /* reserved 3-bit */
static const uint8_t k_ecc_101 = 0x50; /* Micron 7-8 */
static const uint8_t k_ecc_110 = 0x60; /* reserved 3-bit */
static const uint8_t k_ecc_111 = 0x70; /* reserved 3-bit */

TEST_CASE("2-bit C0h packs into nand_ecc_status_t", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_pack_decode_2bit(k_ecc_00) == NAND_ECC_OK);
    REQUIRE(nand_ecc_pack_decode_2bit(k_ecc_01) == NAND_ECC_1_TO_3_BITS_CORRECTED);
    REQUIRE(nand_ecc_pack_decode_2bit(k_ecc_10) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(nand_ecc_pack_decode_2bit(k_ecc_11) == NAND_ECC_4_TO_6_BITS_CORRECTED);
}

TEST_CASE("3-bit C0h packs into Micron-shaped nand_ecc_status_t", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_pack_decode_3bit(k_ecc_00) == NAND_ECC_OK);
    REQUIRE(nand_ecc_pack_decode_3bit(k_ecc_01) == NAND_ECC_1_TO_3_BITS_CORRECTED);
    REQUIRE(nand_ecc_pack_decode_3bit(k_ecc_10) == NAND_ECC_NOT_CORRECTED);
    REQUIRE(nand_ecc_pack_decode_3bit(k_ecc_11) == NAND_ECC_4_TO_6_BITS_CORRECTED);
    REQUIRE(nand_ecc_pack_decode_3bit(k_ecc_101) == NAND_ECC_7_8_BITS_CORRECTED);
    REQUIRE(nand_ecc_pack_decode_3bit(k_ecc_100) == NAND_ECC_MAX);
    REQUIRE(nand_ecc_pack_decode_3bit(k_ecc_110) == NAND_ECC_MAX);
    REQUIRE(nand_ecc_pack_decode_3bit(k_ecc_111) == NAND_ECC_MAX);
}

TEST_CASE("GD reads F0h only when ECCS is 01b", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_gd_needs_status_ext(k_ecc_01) == true);
    REQUIRE(nand_ecc_gd_needs_status_ext(k_ecc_00) == false);
    REQUIRE(nand_ecc_gd_needs_status_ext(k_ecc_10) == false);
    REQUIRE(nand_ecc_gd_needs_status_ext(k_ecc_11) == false);
}

TEST_CASE("GD ECCS 00/10 ignore stale F0h", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_gd_eccse(k_ecc_00, k_ecc_11, true) == NAND_ECC_OK);
    REQUIRE(nand_ecc_decode_gd_eccse(k_ecc_10, k_ecc_11, true) == NAND_ECC_NOT_CORRECTED);
}

TEST_CASE("GD ECCS 11 maps to exactly 8 without using F0h", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_gd_eccse(k_ecc_11, k_ecc_00, true) == NAND_ECC_8_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_gd_eccse(k_ecc_11, k_ecc_11, false) == NAND_ECC_8_BITS_CORRECTED);
}

TEST_CASE("GD ECCS 01 uses exact ECCSE counts", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_gd_eccse(k_ecc_01, k_ecc_00, true) == NAND_ECC_1_TO_4_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_gd_eccse(k_ecc_01, k_ecc_01, true) == NAND_ECC_5_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_gd_eccse(k_ecc_01, k_ecc_10, true) == NAND_ECC_6_BITS_CORRECTED);
    REQUIRE(nand_ecc_decode_gd_eccse(k_ecc_01, k_ecc_11, true) == NAND_ECC_7_BITS_CORRECTED);
}

TEST_CASE("GD failed F0h read forces mid bucket, not uncorrectable", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_decode_gd_eccse(k_ecc_01, 0xFF, false) == NAND_ECC_4_TO_6_BITS_CORRECTED);
}

TEST_CASE("refresh threshold uses min bits, treating GD 1-4 as 4", "[spi_nand_flash][ecc]")
{
    REQUIRE(nand_ecc_min_bits_corrected(NAND_ECC_OK) == 0);
    REQUIRE(nand_ecc_min_bits_corrected(NAND_ECC_1_TO_3_BITS_CORRECTED) == 1);
    REQUIRE(nand_ecc_min_bits_corrected(NAND_ECC_NOT_CORRECTED) == 0);
    REQUIRE(nand_ecc_min_bits_corrected(NAND_ECC_1_TO_4_BITS_CORRECTED) == 4);
    REQUIRE(nand_ecc_min_bits_corrected(NAND_ECC_4_TO_6_BITS_CORRECTED) == 4);
    REQUIRE(nand_ecc_min_bits_corrected(NAND_ECC_5_BITS_CORRECTED) == 5);
    REQUIRE(nand_ecc_min_bits_corrected(NAND_ECC_6_BITS_CORRECTED) == 6);
    REQUIRE(nand_ecc_min_bits_corrected(NAND_ECC_7_BITS_CORRECTED) == 7);
    REQUIRE(nand_ecc_min_bits_corrected(NAND_ECC_7_8_BITS_CORRECTED) == 7);
    REQUIRE(nand_ecc_min_bits_corrected(NAND_ECC_8_BITS_CORRECTED) == 8);
    REQUIRE(nand_ecc_min_bits_corrected(NAND_ECC_MAX) == 0);
}
