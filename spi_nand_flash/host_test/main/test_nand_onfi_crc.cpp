/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>

#include "spi_nand_flash_test_helpers.h"
#include "onfi_param_page_fixtures.h"

#include <catch2/catch_test_macros.hpp>

/* ONFI 1.0 parameter page CRC-16: polynomial 0x8005, init 0x4F4E, bytes 0-253 vs 254-255 LE. */

TEST_CASE("ONFI parameter page CRC golden vectors", "[onfi_crc]")
{
    SECTION("valid fixture passes CRC check") {
        REQUIRE(spi_nand_test_onfi_param_page_crc_valid(onfi_param_page_valid, SPI_NAND_TEST_ONFI_PARAM_PAGE_SIZE));
        REQUIRE(spi_nand_test_onfi_param_page_crc16(onfi_param_page_valid, SPI_NAND_TEST_ONFI_PARAM_PAGE_SIZE - 2) == 0x1BEC);
    }

    SECTION("corrupted CRC fails") {
        REQUIRE_FALSE(spi_nand_test_onfi_param_page_crc_valid(onfi_param_page_bad_crc, SPI_NAND_TEST_ONFI_PARAM_PAGE_SIZE));
    }

    SECTION("bad signature still fails CRC when stored CRC does not match payload") {
        REQUIRE_FALSE(spi_nand_test_onfi_param_page_crc_valid(onfi_param_page_bad_signature, SPI_NAND_TEST_ONFI_PARAM_PAGE_SIZE));
    }

    SECTION("single-byte payload change invalidates CRC") {
        uint8_t page[SPI_NAND_TEST_ONFI_PARAM_PAGE_SIZE];
        memcpy(page, onfi_param_page_valid, sizeof(page));
        page[10] ^= 0x01;
        REQUIRE_FALSE(spi_nand_test_onfi_param_page_crc_valid(page, SPI_NAND_TEST_ONFI_PARAM_PAGE_SIZE));
    }

    SECTION("undersized buffer rejected") {
        REQUIRE_FALSE(spi_nand_test_onfi_param_page_crc_valid(onfi_param_page_valid, SPI_NAND_TEST_ONFI_PARAM_PAGE_SIZE - 1));
    }
}
