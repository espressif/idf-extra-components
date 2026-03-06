/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "spi_nand_flash.h"
#include "spi_nand_flash_test_helpers.h"
#include "nand_linux_mmap_emul.h"
#include "esp_blockdev.h"
#include "esp_nand_blockdev.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("verify mark_bad_block works with bdl interface", "[spi_nand_flash][bdl]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, true};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t nand_bdl;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &nand_bdl) == 0);

    uint32_t block_size = nand_bdl->geometry.erase_size;
    uint32_t block_num = nand_bdl->geometry.disk_size / block_size;

    uint32_t test_block = 15;
    REQUIRE((test_block < block_num) == true);
    REQUIRE(nand_bdl->ops->erase(nand_bdl, test_block * block_size, block_size) == 0);
    // Verify if test_block is not bad block
    esp_blockdev_cmd_arg_is_bad_block_t bad_block_status = {test_block, false};
    REQUIRE(nand_bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_IS_BAD_BLOCK, &bad_block_status) == 0);
    REQUIRE(bad_block_status.status == false);
    // mark test_block as a bad block
    uint32_t block = test_block;
    REQUIRE(nand_bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_MARK_BAD_BLOCK, &block) == 0);
    // Verify if test_block is marked as bad block
    REQUIRE(nand_bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_IS_BAD_BLOCK, &bad_block_status) == 0);
    REQUIRE(bad_block_status.status == true);

    nand_bdl->ops->release(nand_bdl);
}

TEST_CASE("verify nand_prog, nand_read, nand_copy, nand_is_free works with bdl interface", "[spi_nand_flash][bdl]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t nand_bdl;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &nand_bdl) == 0);

    uint32_t block_size = nand_bdl->geometry.erase_size;
    uint32_t sector_size = nand_bdl->geometry.write_size;
    uint32_t sector_num = nand_bdl->geometry.disk_size / sector_size;

    uint8_t *pattern_buf = (uint8_t *)malloc(sector_size);
    REQUIRE(pattern_buf != NULL);
    uint8_t *temp_buf = (uint8_t *)malloc(sector_size);
    REQUIRE(temp_buf != NULL);

    spi_nand_flash_fill_buffer(pattern_buf, sector_size / sizeof(uint32_t));

    uint32_t test_block = 20;
    uint32_t test_page = test_block * (block_size / sector_size); //(block_num * pages_per_block)

    REQUIRE((test_page < sector_num) == true);
    // Verify if test_page is free
    esp_blockdev_cmd_arg_is_free_page_t page_free_status = {test_page, true};
    REQUIRE(nand_bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_IS_FREE_PAGE, &page_free_status) == 0);
    REQUIRE(page_free_status.status == true);
    // Write/program test_page
    REQUIRE(nand_bdl->ops->write(nand_bdl, pattern_buf, test_page * sector_size, sector_size) == 0);
    // Verify if test_page is used/programmed
    REQUIRE(nand_bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_IS_FREE_PAGE, &page_free_status) == 0);
    REQUIRE(page_free_status.status == false);

    REQUIRE(nand_bdl->ops->read(nand_bdl, temp_buf, sector_size, test_page * sector_size, sector_size) == 0);
    REQUIRE(spi_nand_flash_check_buffer(temp_buf, sector_size / sizeof(uint32_t)) == 0);

    free(pattern_buf);
    free(temp_buf);
    nand_bdl->ops->release(nand_bdl);
}

