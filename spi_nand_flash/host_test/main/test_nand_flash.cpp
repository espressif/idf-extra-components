/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "spi_nand_flash.h"
#include "nand_linux_mmap_emul.h"
#include "nand_private/nand_impl_wrap.h"

#include <catch2/catch_test_macros.hpp>

#define PATTERN_SEED    0x12345678

TEST_CASE("verify mark_bad_block works", "[spi_nand_flash]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, true};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    spi_nand_flash_device_t *device_handle;
    REQUIRE(spi_nand_flash_init_device(&nand_flash_config, &device_handle) == ESP_OK);

    uint32_t sector_num, sector_size;
    REQUIRE(spi_nand_flash_get_capacity(device_handle, &sector_num) == 0);
    REQUIRE(spi_nand_flash_get_sector_size(device_handle, &sector_size) == 0);

    uint32_t test_block = 15;
    if (test_block < sector_num) {
        bool is_bad_status = false;
        // Verify if test_block is not bad block
        REQUIRE(nand_wrap_is_bad(device_handle, test_block, &is_bad_status) == 0);
        REQUIRE(is_bad_status == false);
        // mark test_block as a bad block
        REQUIRE(nand_wrap_mark_bad(device_handle, test_block) == 0);
        // Verify if test_block is marked as bad block
        REQUIRE(nand_wrap_is_bad(device_handle, test_block, &is_bad_status) == 0);
        REQUIRE(is_bad_status == true);
    }

    spi_nand_flash_deinit_device(device_handle);
}

static void fill_buffer(uint32_t seed, uint8_t *dst, size_t count)
{
    srand(seed);
    for (size_t i = 0; i < count; ++i) {
        uint32_t val = rand();
        memcpy(dst + i * sizeof(uint32_t), &val, sizeof(val));
    }
}

TEST_CASE("verify nand_prog, nand_read, nand_copy, nand_is_free works", "[spi_nand_flash]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    spi_nand_flash_device_t *device_handle;
    REQUIRE(spi_nand_flash_init_device(&nand_flash_config, &device_handle) == ESP_OK);

    uint32_t sector_num, sector_size, block_size;
    REQUIRE(spi_nand_flash_get_capacity(device_handle, &sector_num) == 0);
    REQUIRE(spi_nand_flash_get_sector_size(device_handle, &sector_size) == 0);
    REQUIRE(spi_nand_flash_get_block_size(device_handle, &block_size) == 0);

    uint8_t *pattern_buf = (uint8_t *)malloc(sector_size);
    REQUIRE(pattern_buf != NULL);
    uint8_t *temp_buf = (uint8_t *)malloc(sector_size);
    REQUIRE(temp_buf != NULL);

    fill_buffer(PATTERN_SEED, pattern_buf, sector_size / sizeof(uint32_t));

    bool is_page_free = true;
    uint32_t test_block = 20;
    uint32_t test_page = test_block * (block_size / sector_size); //(block_num * pages_per_block)
    uint32_t dst_page = test_page + 1;
    if (test_page < sector_num) {
        // Verify if test_page is free
        REQUIRE(nand_wrap_is_free(device_handle, test_page, &is_page_free) == 0);
        REQUIRE(is_page_free == true);
        // Write/program test_page
        REQUIRE(nand_wrap_prog(device_handle, test_page, pattern_buf) == 0);
        // Verify if test_page is used/programmed
        REQUIRE(nand_wrap_is_free(device_handle, test_page, &is_page_free) == 0);
        REQUIRE(is_page_free == false);

        REQUIRE(nand_wrap_read(device_handle, test_page, 0, sector_size, temp_buf) == 0);
        REQUIRE(nand_wrap_copy(device_handle, test_page, dst_page) == 0);
        REQUIRE(nand_wrap_read(device_handle, dst_page, 0, sector_size, temp_buf) == 0);
    }
    free(pattern_buf);
    free(temp_buf);
    spi_nand_flash_deinit_device(device_handle);
}
