/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cstdlib>

#include "spi_nand_flash.h"
#include "spi_nand_flash_test_helpers.h"
#include "nand_linux_mmap_emul.h"
#include "esp_blockdev.h"
#include "esp_nand_blockdev.h"

#include <catch2/catch_test_macros.hpp>

/* Linux mmap file: each page is k_page data + k_oob, so the file uses k_ppb * (k_page + k_oob)
 * bytes per erase block. chip.block_size is k_ppb * k_page (same as real chips); num_blocks
 * is file_size / (k_ppb * (k_page + k_oob)). BDL disk_size must be num_blocks * chip.block_size.
 * Regression: storing the file stride in chip.block_size broke BDL erase decode. */
TEST_CASE("BDL geometry matches Linux mmap file and chip.block_size", "[spi_nand_flash][bdl]")
{
    constexpr uint32_t k_file_bytes = 16u * 1024u * 1024u;
    constexpr uint32_t k_page = 2048u;
    constexpr uint32_t k_oob = 64u;
    constexpr uint32_t k_ppb = 64u;
    const uint32_t file_bytes_per_physical_block = k_ppb * (k_page + k_oob);
    const uint32_t bdl_bytes_per_erase_block = k_ppb * k_page;
    const uint32_t num_physical_blocks = k_file_bytes / file_bytes_per_physical_block;
    const uint64_t expected_disk_size = (uint64_t)num_physical_blocks * bdl_bytes_per_erase_block;

    nand_file_mmap_emul_config_t conf = {"", k_file_bytes, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &bdl) == ESP_OK);
    REQUIRE(bdl->geometry.write_size == k_page);
    REQUIRE(bdl->geometry.erase_size == bdl_bytes_per_erase_block);
    REQUIRE(bdl->geometry.disk_size == expected_disk_size);
    bdl->ops->release(bdl);
}

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

TEST_CASE("WL BDL on host: create, geometry, write/read, release", "[spi_nand_flash][bdl]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t flash_bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &flash_bdl) == ESP_OK);
    REQUIRE(flash_bdl != nullptr);

    esp_blockdev_handle_t wl_bdl = nullptr;
    REQUIRE(spi_nand_flash_wl_get_blockdev(flash_bdl, &wl_bdl) == ESP_OK);
    REQUIRE(wl_bdl != nullptr);

    REQUIRE(wl_bdl->geometry.disk_size > 0);
    REQUIRE(wl_bdl->geometry.read_size > 0);
    REQUIRE(wl_bdl->geometry.write_size > 0);
    REQUIRE(wl_bdl->geometry.erase_size > 0);

    uint32_t page_size = wl_bdl->geometry.write_size;
    REQUIRE(page_size > 0);
    uint32_t num_pages = (uint32_t)(wl_bdl->geometry.disk_size / page_size);
    REQUIRE(num_pages > 0);

    uint8_t *pattern_buf = (uint8_t *)malloc(page_size);
    uint8_t *read_buf = (uint8_t *)malloc(page_size);
    REQUIRE(pattern_buf != nullptr);
    REQUIRE(read_buf != nullptr);
    spi_nand_flash_fill_buffer(pattern_buf, page_size / sizeof(uint32_t));

    REQUIRE(wl_bdl->ops->write(wl_bdl, pattern_buf, 0, page_size) == ESP_OK);
    memset(read_buf, 0, page_size);
    REQUIRE(wl_bdl->ops->read(wl_bdl, read_buf, page_size, 0, page_size) == ESP_OK);
    REQUIRE(spi_nand_flash_check_buffer(read_buf, page_size / sizeof(uint32_t)) == 0);

    free(pattern_buf);
    free(read_buf);
    wl_bdl->ops->release(wl_bdl);
}

TEST_CASE("Flash BDL geometry and ops on host", "[spi_nand_flash][bdl]")
{
    nand_file_mmap_emul_config_t conf = {"", 20 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &bdl) == ESP_OK);
    REQUIRE(bdl != nullptr);

    uint32_t block_size = bdl->geometry.erase_size;
    uint32_t num_blocks = (uint32_t)(bdl->geometry.disk_size / block_size);
    REQUIRE(bdl->geometry.disk_size == (uint64_t)num_blocks * block_size);
    REQUIRE(bdl->geometry.read_size == bdl->geometry.write_size);
    REQUIRE(bdl->geometry.read_size > 0);
    REQUIRE(bdl->geometry.erase_size > 0);

    REQUIRE(bdl->ops != nullptr);
    REQUIRE(bdl->ops->read != nullptr);
    REQUIRE(bdl->ops->write != nullptr);
    REQUIRE(bdl->ops->erase != nullptr);
    REQUIRE(bdl->ops->ioctl != nullptr);
    REQUIRE(bdl->ops->release != nullptr);

    bdl->ops->release(bdl);
}

TEST_CASE("Flash BDL COPY_PAGE ioctl on host", "[spi_nand_flash][bdl]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t nand_bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &nand_bdl) == ESP_OK);
    REQUIRE(nand_bdl != nullptr);

    uint32_t block_size = nand_bdl->geometry.erase_size;
    uint32_t page_size = nand_bdl->geometry.write_size;
    uint32_t pages_per_block = block_size / page_size;
    uint32_t src_page = 5 * pages_per_block;
    uint32_t dst_page = 6 * pages_per_block;

    REQUIRE(nand_bdl->ops->erase(nand_bdl, src_page * (uint64_t)page_size, block_size) == ESP_OK);

    uint8_t *pattern_buf = (uint8_t *)malloc(page_size);
    uint8_t *read_buf = (uint8_t *)malloc(page_size);
    REQUIRE(pattern_buf != nullptr);
    REQUIRE(read_buf != nullptr);
    spi_nand_flash_fill_buffer(pattern_buf, page_size / sizeof(uint32_t));

    REQUIRE(nand_bdl->ops->write(nand_bdl, pattern_buf, src_page * (uint64_t)page_size, page_size) == ESP_OK);

    esp_blockdev_cmd_arg_copy_page_t copy_cmd = { .src_page = src_page, .dst_page = dst_page };
    REQUIRE(nand_bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_COPY_PAGE, &copy_cmd) == ESP_OK);

    memset(read_buf, 0, page_size);
    REQUIRE(nand_bdl->ops->read(nand_bdl, read_buf, page_size, dst_page * (uint64_t)page_size, page_size) == ESP_OK);
    REQUIRE(spi_nand_flash_check_buffer(read_buf, page_size / sizeof(uint32_t)) == 0);

    free(pattern_buf);
    free(read_buf);
    nand_bdl->ops->release(nand_bdl);
}

TEST_CASE("Flash BDL GET_NAND_FLASH_INFO and GET_BAD_BLOCKS_COUNT on host", "[spi_nand_flash][bdl]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t nand_bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &nand_bdl) == ESP_OK);
    REQUIRE(nand_bdl != nullptr);

    esp_blockdev_cmd_arg_nand_flash_info_t flash_info;
    memset(&flash_info, 0, sizeof(flash_info));
    REQUIRE(nand_bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_GET_NAND_FLASH_INFO, &flash_info) == ESP_OK);
    REQUIRE(strnlen((const char *)flash_info.device_info.chip_name, sizeof(flash_info.device_info.chip_name)) > 0);
    uint32_t total_blocks = nand_bdl->geometry.disk_size / nand_bdl->geometry.erase_size;
    REQUIRE(flash_info.geometry.num_blocks == total_blocks);

    uint32_t bad_block_count = 0xFFFF;
    REQUIRE(nand_bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_GET_BAD_BLOCKS_COUNT, &bad_block_count) == ESP_OK);
    REQUIRE(bad_block_count <= total_blocks);

    nand_bdl->ops->release(nand_bdl);
}

TEST_CASE("Error path: nand_flash_get_blockdev NULL/invalid args", "[spi_nand_flash][bdl]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, false};
    spi_nand_flash_config_t config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t out = nullptr;

    REQUIRE(nand_flash_get_blockdev(nullptr, &out) == ESP_ERR_INVALID_ARG);
    REQUIRE(out == nullptr);
    REQUIRE(nand_flash_get_blockdev(&config, nullptr) == ESP_ERR_INVALID_ARG);
}

TEST_CASE("Release and no use-after-free: create, release, create again, minimal r/w", "[spi_nand_flash][bdl]")
{
    nand_file_mmap_emul_config_t conf = {"", 20 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};

    esp_blockdev_handle_t bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &bdl) == ESP_OK);
    REQUIRE(bdl != nullptr);
    bdl->ops->release(bdl);

    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &bdl) == ESP_OK);
    REQUIRE(bdl != nullptr);

    uint32_t page_size = bdl->geometry.write_size;
    uint8_t *buf = (uint8_t *)malloc(page_size);
    REQUIRE(buf != nullptr);
    spi_nand_flash_fill_buffer(buf, page_size / sizeof(uint32_t));
    REQUIRE(bdl->ops->write(bdl, buf, 0, page_size) == ESP_OK);
    memset(buf, 0, page_size);
    REQUIRE(bdl->ops->read(bdl, buf, page_size, 0, page_size) == ESP_OK);
    REQUIRE(spi_nand_flash_check_buffer(buf, page_size / sizeof(uint32_t)) == 0);

    free(buf);
    bdl->ops->release(bdl);
}

TEST_CASE("Flash BDL last physical page write/read", "[spi_nand_flash][bdl][raw]")
{
    nand_file_mmap_emul_config_t conf = {"", 16 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &bdl) == ESP_OK);

    const uint32_t page_size = bdl->geometry.write_size;
    const uint32_t block_size = bdl->geometry.erase_size;
    const uint64_t disk_size = bdl->geometry.disk_size;
    const uint32_t num_pages = (uint32_t)(disk_size / page_size);
    const uint32_t pages_per_block = block_size / page_size;
    const uint32_t last_page = num_pages - 1u;
    const uint32_t last_block = last_page / pages_per_block;
    const uint64_t block_addr = (uint64_t)last_block * block_size;

    REQUIRE(bdl->ops->erase(bdl, block_addr, block_size) == ESP_OK);

    uint8_t *w = (uint8_t *)malloc(page_size);
    uint8_t *r = (uint8_t *)malloc(page_size);
    REQUIRE(w != nullptr);
    REQUIRE(r != nullptr);

    spi_nand_flash_fill_buffer(w, page_size / sizeof(uint32_t));
    const uint64_t last_off = (uint64_t)last_page * page_size;
    REQUIRE(bdl->ops->write(bdl, w, last_off, page_size) == ESP_OK);
    memset(r, 0, page_size);
    REQUIRE(bdl->ops->read(bdl, r, page_size, last_off, page_size) == ESP_OK);
    REQUIRE(spi_nand_flash_check_buffer(r, page_size / sizeof(uint32_t)) == 0);

    free(w);
    free(r);
    bdl->ops->release(bdl);
}

TEST_CASE("Flash BDL 16 MiB full erase+program sweep then read-back all pages", "[spi_nand_flash][bdl][raw][sequential]")
{
    nand_file_mmap_emul_config_t conf = {"", 16 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &bdl) == ESP_OK);

    const uint32_t page_size = bdl->geometry.write_size;
    const uint32_t block_size = bdl->geometry.erase_size;
    const uint64_t disk_size = bdl->geometry.disk_size;
    const uint32_t num_blocks = (uint32_t)(disk_size / block_size);
    const uint32_t pages_per_block = block_size / page_size;

    for (uint32_t b = 0; b < num_blocks; b++) {
        const uint64_t ba = (uint64_t)b * block_size;
        REQUIRE(bdl->ops->erase(bdl, ba, block_size) == ESP_OK);
        for (uint32_t i = 0; i < pages_per_block; i++) {
            const uint32_t page = b * pages_per_block + i;
            uint8_t *w = (uint8_t *)malloc(page_size);
            REQUIRE(w != nullptr);
            spi_nand_flash_fill_buffer(w, page_size / sizeof(uint32_t));
            REQUIRE(bdl->ops->write(bdl, w, (uint64_t)page * page_size, page_size) == ESP_OK);
            free(w);
        }
    }

    const uint32_t num_pages = (uint32_t)(disk_size / page_size);
    uint8_t *r = (uint8_t *)malloc(page_size);
    REQUIRE(r != nullptr);
    for (uint32_t page = 0; page < num_pages; page++) {
        memset(r, 0, page_size);
        REQUIRE(bdl->ops->read(bdl, r, page_size, (uint64_t)page * page_size, page_size) == ESP_OK);
        REQUIRE(spi_nand_flash_check_buffer(r, page_size / sizeof(uint32_t)) == 0);
    }
    free(r);
    bdl->ops->release(bdl);
}

TEST_CASE("Flash BDL overwrite same physical page many times", "[spi_nand_flash][bdl][raw][stress]")
{
    nand_file_mmap_emul_config_t conf = {"", 20 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &bdl) == ESP_OK);

    const uint32_t page_size = bdl->geometry.write_size;
    const uint32_t block_size = bdl->geometry.erase_size;
    REQUIRE(bdl->ops->erase(bdl, 0, block_size) == ESP_OK);

    uint8_t *w = (uint8_t *)malloc(page_size);
    uint8_t *r = (uint8_t *)malloc(page_size);
    REQUIRE(w != nullptr);
    REQUIRE(r != nullptr);

    constexpr int kRounds = 150;
    for (int i = 0; i < kRounds; i++) {
        spi_nand_flash_fill_buffer(w, page_size / sizeof(uint32_t));
        REQUIRE(bdl->ops->write(bdl, w, 0, page_size) == ESP_OK);
    }
    spi_nand_flash_fill_buffer(w, page_size / sizeof(uint32_t));
    REQUIRE(bdl->ops->read(bdl, r, page_size, 0, page_size) == ESP_OK);
    REQUIRE(spi_nand_flash_check_buffer(r, page_size / sizeof(uint32_t)) == 0);

    free(w);
    free(r);
    bdl->ops->release(bdl);
}

TEST_CASE("Flash BDL multi-page write in one call", "[spi_nand_flash][bdl][raw]")
{
    nand_file_mmap_emul_config_t conf = {"", 20 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &bdl) == ESP_OK);

    const uint32_t page_size = bdl->geometry.write_size;
    const uint32_t block_size = bdl->geometry.erase_size;
    const uint32_t n = 4;
    REQUIRE(block_size >= n * page_size);

    REQUIRE(bdl->ops->erase(bdl, 0, block_size) == ESP_OK);

    const size_t total = (size_t)n * page_size;
    uint8_t *w = (uint8_t *)malloc(total);
    uint8_t *r = (uint8_t *)malloc(page_size);
    REQUIRE(w != nullptr);
    REQUIRE(r != nullptr);

    for (uint32_t p = 0; p < n; p++) {
        spi_nand_flash_fill_buffer(w + (size_t)p * page_size, page_size / sizeof(uint32_t));
    }
    REQUIRE(bdl->ops->write(bdl, w, 0, total) == ESP_OK);

    for (uint32_t p = 0; p < n; p++) {
        memset(r, 0, page_size);
        REQUIRE(bdl->ops->read(bdl, r, page_size, (uint64_t)p * page_size, page_size) == ESP_OK);
        REQUIRE(spi_nand_flash_check_buffer(r, page_size / sizeof(uint32_t)) == 0);
    }

    free(w);
    free(r);
    bdl->ops->release(bdl);
}

TEST_CASE("Flash BDL sub-page read and misaligned write rejected", "[spi_nand_flash][bdl][raw]")
{
    nand_file_mmap_emul_config_t conf = {"", 20 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &bdl) == ESP_OK);

    const uint32_t page_size = bdl->geometry.write_size;
    const uint32_t block_size = bdl->geometry.erase_size;
    REQUIRE(bdl->ops->erase(bdl, 0, block_size) == ESP_OK);

    uint8_t *full = (uint8_t *)malloc(page_size);
    uint8_t *slice = (uint8_t *)malloc(page_size);
    REQUIRE(full != nullptr);
    REQUIRE(slice != nullptr);
    spi_nand_flash_fill_buffer(full, page_size / sizeof(uint32_t));
    REQUIRE(bdl->ops->write(bdl, full, 0, page_size) == ESP_OK);

    const size_t off = 64;
    const size_t len = page_size - 128;
    REQUIRE(off + len <= page_size);
    memset(slice, 0, len);
    REQUIRE(bdl->ops->read(bdl, slice, len, off, len) == ESP_OK);
    REQUIRE(memcmp(slice, full + off, len) == 0);

    REQUIRE(bdl->ops->write(bdl, full, 1, page_size) == ESP_ERR_INVALID_SIZE);

    free(full);
    free(slice);
    bdl->ops->release(bdl);
}

TEST_CASE("Flash BDL COPY_PAGE same page is idempotent", "[spi_nand_flash][bdl][raw][copy]")
{
    nand_file_mmap_emul_config_t conf = {"", 20 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &bdl) == ESP_OK);

    const uint32_t page_size = bdl->geometry.write_size;
    const uint32_t block_size = bdl->geometry.erase_size;
    const uint32_t page = 3 * (block_size / page_size);

    REQUIRE(bdl->ops->erase(bdl, (page / (block_size / page_size)) * (uint64_t)block_size, block_size) == ESP_OK);

    uint8_t *w = (uint8_t *)malloc(page_size);
    uint8_t *r = (uint8_t *)malloc(page_size);
    REQUIRE(w != nullptr);
    REQUIRE(r != nullptr);
    spi_nand_flash_fill_buffer(w, page_size / sizeof(uint32_t));
    const uint64_t addr = (uint64_t)page * page_size;
    REQUIRE(bdl->ops->write(bdl, w, addr, page_size) == ESP_OK);

    esp_blockdev_cmd_arg_copy_page_t copy_cmd = {.src_page = page, .dst_page = page};
    REQUIRE(bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_COPY_PAGE, &copy_cmd) == ESP_OK);

    memset(r, 0, page_size);
    REQUIRE(bdl->ops->read(bdl, r, page_size, addr, page_size) == ESP_OK);
    REQUIRE(spi_nand_flash_check_buffer(r, page_size / sizeof(uint32_t)) == 0);

    free(w);
    free(r);
    bdl->ops->release(bdl);
}

TEST_CASE("Flash BDL COPY_PAGE does not alter source page", "[spi_nand_flash][bdl][raw][copy]")
{
    nand_file_mmap_emul_config_t conf = {"", 20 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &bdl) == ESP_OK);

    const uint32_t page_size = bdl->geometry.write_size;
    const uint32_t block_size = bdl->geometry.erase_size;
    const uint32_t ppb = block_size / page_size;
    const uint32_t src_page = 2u * ppb;
    const uint32_t dst_page = 2u * ppb + 1u;

    REQUIRE(bdl->ops->erase(bdl, 2u * (uint64_t)block_size, block_size) == ESP_OK);

    uint8_t *w = (uint8_t *)malloc(page_size);
    uint8_t *r = (uint8_t *)malloc(page_size);
    REQUIRE(w != nullptr);
    REQUIRE(r != nullptr);
    spi_nand_flash_fill_buffer(w, page_size / sizeof(uint32_t));
    REQUIRE(bdl->ops->write(bdl, w, (uint64_t)src_page * page_size, page_size) == ESP_OK);

    esp_blockdev_cmd_arg_copy_page_t copy_cmd = {.src_page = src_page, .dst_page = dst_page};
    REQUIRE(bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_COPY_PAGE, &copy_cmd) == ESP_OK);

    memset(r, 0, page_size);
    REQUIRE(bdl->ops->read(bdl, r, page_size, (uint64_t)src_page * page_size, page_size) == ESP_OK);
    REQUIRE(memcmp(w, r, page_size) == 0);

    free(w);
    free(r);
    bdl->ops->release(bdl);
}

TEST_CASE("Flash BDL invalid write/read geometry returns error", "[spi_nand_flash][bdl][raw]")
{
    nand_file_mmap_emul_config_t conf = {"", 20 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &bdl) == ESP_OK);

    const uint32_t page_size = bdl->geometry.write_size;
    const uint64_t disk = bdl->geometry.disk_size;
    uint8_t *buf = (uint8_t *)malloc(page_size * 2);
    REQUIRE(buf != nullptr);
    spi_nand_flash_fill_buffer(buf, page_size / sizeof(uint32_t));

    REQUIRE(bdl->ops->write(bdl, buf, page_size / 2u, page_size) == ESP_ERR_INVALID_SIZE);
    if (page_size >= 512u) {
        REQUIRE(bdl->ops->write(bdl, buf, 0, page_size / 2u) == ESP_ERR_INVALID_SIZE);
    }

    memset(buf, 0, page_size);
    REQUIRE(bdl->ops->read(bdl, buf, page_size, disk, page_size) == ESP_ERR_INVALID_SIZE);
    REQUIRE(bdl->ops->write(bdl, buf, disk, page_size) == ESP_ERR_INVALID_SIZE);

    free(buf);
    bdl->ops->release(bdl);
}

TEST_CASE("Flash BDL GET_PAGE_ECC_STATUS and GET_ECC_STATS (small image)", "[spi_nand_flash][bdl][raw]")
{
    nand_file_mmap_emul_config_t conf = {"", 4 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &bdl) == ESP_OK);

    const uint32_t page_size = bdl->geometry.write_size;
    const uint32_t block_size = bdl->geometry.erase_size;
    REQUIRE(bdl->ops->erase(bdl, 0, block_size) == ESP_OK);

    uint8_t *buf = (uint8_t *)malloc(page_size);
    REQUIRE(buf != nullptr);
    spi_nand_flash_fill_buffer(buf, page_size / sizeof(uint32_t));
    REQUIRE(bdl->ops->write(bdl, buf, 0, page_size) == ESP_OK);

    esp_blockdev_cmd_arg_ecc_status_t ecc_page = {};
    ecc_page.page_num = 0;
    REQUIRE(bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_PAGE_ECC_STATUS, &ecc_page) == ESP_OK);

    esp_blockdev_cmd_arg_ecc_stats_t ecc_stats = {};
    REQUIRE(bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_ECC_STATS, &ecc_stats) == ESP_OK);

    free(buf);
    bdl->ops->release(bdl);
}

TEST_CASE("WL BDL sync after write preserves data", "[spi_nand_flash][bdl][wl]")
{
    nand_file_mmap_emul_config_t conf = {"", 16 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t flash_bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &flash_bdl) == ESP_OK);
    esp_blockdev_handle_t wl_bdl = nullptr;
    REQUIRE(spi_nand_flash_wl_get_blockdev(flash_bdl, &wl_bdl) == ESP_OK);

    const uint32_t page_size = wl_bdl->geometry.write_size;
    uint8_t *w = (uint8_t *)malloc(page_size);
    uint8_t *r = (uint8_t *)malloc(page_size);
    REQUIRE(w != nullptr);
    REQUIRE(r != nullptr);

    spi_nand_flash_fill_buffer(w, page_size / sizeof(uint32_t));
    REQUIRE(wl_bdl->ops->write(wl_bdl, w, page_size, page_size) == ESP_OK);
    REQUIRE(wl_bdl->ops->sync(wl_bdl) == ESP_OK);
    memset(r, 0, page_size);
    REQUIRE(wl_bdl->ops->read(wl_bdl, r, page_size, page_size, page_size) == ESP_OK);
    REQUIRE(spi_nand_flash_check_buffer(r, page_size / sizeof(uint32_t)) == 0);

    free(w);
    free(r);
    wl_bdl->ops->release(wl_bdl);
}

TEST_CASE("WL BDL MARK_DELETED then rewrite same logical page", "[spi_nand_flash][bdl][wl]")
{
    nand_file_mmap_emul_config_t conf = {"", 16 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t flash_bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &flash_bdl) == ESP_OK);
    esp_blockdev_handle_t wl_bdl = nullptr;
    REQUIRE(spi_nand_flash_wl_get_blockdev(flash_bdl, &wl_bdl) == ESP_OK);

    const uint32_t page_size = wl_bdl->geometry.write_size;
    const uint32_t page_index = 5;
    const uint64_t off = (uint64_t)page_index * page_size;

    uint8_t *w = (uint8_t *)malloc(page_size);
    uint8_t *r = (uint8_t *)malloc(page_size);
    REQUIRE(w != nullptr);
    REQUIRE(r != nullptr);

    memset(w, 0x22, page_size);
    REQUIRE(wl_bdl->ops->write(wl_bdl, w, off, page_size) == ESP_OK);

    esp_blockdev_cmd_arg_erase_t trim_arg = {.start_addr = off, .erase_len = page_size};
    REQUIRE(wl_bdl->ops->ioctl(wl_bdl, ESP_BLOCKDEV_CMD_MARK_DELETED, &trim_arg) == ESP_OK);

    spi_nand_flash_fill_buffer(w, page_size / sizeof(uint32_t));
    REQUIRE(wl_bdl->ops->write(wl_bdl, w, off, page_size) == ESP_OK);
    memset(r, 0, page_size);
    REQUIRE(wl_bdl->ops->read(wl_bdl, r, page_size, off, page_size) == ESP_OK);
    REQUIRE(spi_nand_flash_check_buffer(r, page_size / sizeof(uint32_t)) == 0);

    free(w);
    free(r);
    wl_bdl->ops->release(wl_bdl);
}

TEST_CASE("WL BDL MARK_DELETED misaligned range returns ESP_ERR_INVALID_SIZE", "[spi_nand_flash][bdl][wl]")
{
    nand_file_mmap_emul_config_t conf = {"", 20 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t flash_bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &flash_bdl) == ESP_OK);
    esp_blockdev_handle_t wl_bdl = nullptr;
    REQUIRE(spi_nand_flash_wl_get_blockdev(flash_bdl, &wl_bdl) == ESP_OK);

    const uint32_t page_size = wl_bdl->geometry.write_size;
    esp_blockdev_cmd_arg_erase_t bad = {.start_addr = 1, .erase_len = page_size};
    REQUIRE(wl_bdl->ops->ioctl(wl_bdl, ESP_BLOCKDEV_CMD_MARK_DELETED, &bad) == ESP_ERR_INVALID_SIZE);

    wl_bdl->ops->release(wl_bdl);
}

TEST_CASE("WL BDL 16 MiB sequential logical page fill and read-back", "[spi_nand_flash][bdl][wl][sequential]")
{
    nand_file_mmap_emul_config_t conf = {"", 16 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t flash_bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &flash_bdl) == ESP_OK);
    esp_blockdev_handle_t wl_bdl = nullptr;
    REQUIRE(spi_nand_flash_wl_get_blockdev(flash_bdl, &wl_bdl) == ESP_OK);

    const uint32_t page_size = wl_bdl->geometry.write_size;
    const uint32_t num_pages = (uint32_t)(wl_bdl->geometry.disk_size / page_size);
    REQUIRE(wl_bdl->geometry.disk_size == (uint64_t)num_pages * page_size);

    uint8_t *w = (uint8_t *)malloc(page_size);
    REQUIRE(w != nullptr);

    for (uint32_t p = 0; p < num_pages; p++) {
        spi_nand_flash_fill_buffer(w, page_size / sizeof(uint32_t));
        REQUIRE(wl_bdl->ops->write(wl_bdl, w, (uint64_t)p * page_size, page_size) == ESP_OK);
    }
    REQUIRE(wl_bdl->ops->sync(wl_bdl) == ESP_OK);

    uint8_t *r = (uint8_t *)malloc(page_size);
    REQUIRE(r != nullptr);
    for (uint32_t p = 0; p < num_pages; p++) {
        memset(r, 0, page_size);
        REQUIRE(wl_bdl->ops->read(wl_bdl, r, page_size, (uint64_t)p * page_size, page_size) == ESP_OK);
        REQUIRE(spi_nand_flash_check_buffer(r, page_size / sizeof(uint32_t)) == 0);
    }

    free(w);
    free(r);
    wl_bdl->ops->release(wl_bdl);
}

TEST_CASE("WL BDL hot-set random writes then read-back", "[spi_nand_flash][bdl][wl][stress]")
{
    nand_file_mmap_emul_config_t conf = {"", 16 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t flash_bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &flash_bdl) == ESP_OK);
    esp_blockdev_handle_t wl_bdl = nullptr;
    REQUIRE(spi_nand_flash_wl_get_blockdev(flash_bdl, &wl_bdl) == ESP_OK);

    constexpr uint32_t kHotSetSize = 30u;
    constexpr uint32_t kTotalWrites = 1200u;

    const uint32_t page_size = wl_bdl->geometry.write_size;
    const uint32_t num_pages = (uint32_t)(wl_bdl->geometry.disk_size / page_size);
    REQUIRE(num_pages >= kHotSetSize);

    uint8_t *w = (uint8_t *)malloc(page_size);
    uint8_t *r = (uint8_t *)malloc(page_size);
    REQUIRE(w != nullptr);
    REQUIRE(r != nullptr);

    bool written[kHotSetSize] = {};

    std::srand(0xDEADBEEFu);
    for (uint32_t op = 0; op < kTotalWrites; op++) {
        const uint32_t lp = (uint32_t)((unsigned)std::rand() % kHotSetSize);
        spi_nand_flash_fill_buffer(w, page_size / sizeof(uint32_t));
        REQUIRE(wl_bdl->ops->write(wl_bdl, w, (uint64_t)lp * page_size, page_size) == ESP_OK);
        written[lp] = true;
    }

    REQUIRE(wl_bdl->ops->sync(wl_bdl) == ESP_OK);

    for (uint32_t s = 0; s < kHotSetSize; s++) {
        if (!written[s]) {
            continue;
        }
        REQUIRE(wl_bdl->ops->read(wl_bdl, r, page_size, (uint64_t)s * page_size, page_size) == ESP_OK);
        REQUIRE(spi_nand_flash_check_buffer(r, page_size / sizeof(uint32_t)) == 0);
    }

    free(w);
    free(r);
    wl_bdl->ops->release(wl_bdl);
}

TEST_CASE("WL BDL hot-set with interleaved MARK_DELETED", "[spi_nand_flash][bdl][wl][stress][trim]")
{
    nand_file_mmap_emul_config_t conf = {"", 16 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t flash_bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &flash_bdl) == ESP_OK);
    esp_blockdev_handle_t wl_bdl = nullptr;
    REQUIRE(spi_nand_flash_wl_get_blockdev(flash_bdl, &wl_bdl) == ESP_OK);

    constexpr uint32_t kHotSetSize = 30u;
    constexpr uint32_t kTotalWrites = 1200u;

    const uint32_t page_size = wl_bdl->geometry.write_size;
    const uint32_t num_pages = (uint32_t)(wl_bdl->geometry.disk_size / page_size);
    REQUIRE(num_pages >= kHotSetSize);

    uint8_t *w = (uint8_t *)malloc(page_size);
    uint8_t *r = (uint8_t *)malloc(page_size);
    REQUIRE(w != nullptr);
    REQUIRE(r != nullptr);

    bool trimmed[kHotSetSize] = {};
    bool written[kHotSetSize] = {};

    std::srand(0xCAFEBABEu);
    for (uint32_t op = 0; op < kTotalWrites; op++) {
        const uint32_t lp = (uint32_t)((unsigned)std::rand() % kHotSetSize);

        if (op % 100u == 99u) {
            const uint32_t t = (uint32_t)((unsigned)std::rand() % kHotSetSize);
            esp_blockdev_cmd_arg_erase_t trim_arg = {
                .start_addr = (uint64_t)t * page_size,
                .erase_len = page_size,
            };
            if (wl_bdl->ops->ioctl(wl_bdl, ESP_BLOCKDEV_CMD_MARK_DELETED, &trim_arg) == ESP_OK) {
                trimmed[t] = true;
            }
        }

        spi_nand_flash_fill_buffer(w, page_size / sizeof(uint32_t));
        REQUIRE(wl_bdl->ops->write(wl_bdl, w, (uint64_t)lp * page_size, page_size) == ESP_OK);
        trimmed[lp] = false;
        written[lp] = true;
    }

    REQUIRE(wl_bdl->ops->sync(wl_bdl) == ESP_OK);

    for (uint32_t s = 0; s < kHotSetSize; s++) {
        if (trimmed[s] || !written[s]) {
            continue;
        }
        REQUIRE(wl_bdl->ops->read(wl_bdl, r, page_size, (uint64_t)s * page_size, page_size) == ESP_OK);
        REQUIRE(spi_nand_flash_check_buffer(r, page_size / sizeof(uint32_t)) == 0);
    }

    free(w);
    free(r);
    wl_bdl->ops->release(wl_bdl);
}
