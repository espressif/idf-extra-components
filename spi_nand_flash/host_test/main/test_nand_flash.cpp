/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "esp_partition.h"
#include "diskio_impl.h"
#include "diskio_nand.h"
#include "spi_nand_flash.h"
#include "nand_private/nand_impl_wrap.h"

#include <catch2/catch_test_macros.hpp>

#define PATTERN_SEED    0x12345678

TEST_CASE("Create volume, open file, write and read back data", "[fatfs, spi_nand_flash]")
{
    FRESULT fr_result;
    BYTE pdrv;
    FATFS fs;
    FIL file;
    UINT bw;

    esp_err_t esp_result;
    spi_nand_flash_config_t nand_flash_config;
    spi_nand_flash_device_t *device_handle;
    REQUIRE(spi_nand_flash_init_device(&nand_flash_config, &device_handle) == ESP_OK);

    // Get a physical drive
    esp_result = ff_diskio_get_drive(&pdrv);
    REQUIRE(esp_result == ESP_OK);

    // Register physical drive as wear-levelled partition
    esp_result = ff_diskio_register_nand(pdrv, device_handle);

    // Create FAT volume on the entire disk
    LBA_t part_list[] = {100, 0, 0, 0};
    BYTE work_area[FF_MAX_SS];

    fr_result = f_fdisk(pdrv, part_list, work_area);
    REQUIRE(fr_result == FR_OK);

    char drv[3] = {(char)('0' + pdrv), ':', 0};
    const MKFS_PARM opt = {(BYTE)(FM_ANY), 0, 0, 0, 0};
    fr_result = f_mkfs(drv, &opt, work_area, sizeof(work_area)); // Use default volume
    REQUIRE(fr_result == FR_OK);

    // Mount the volume
    fr_result = f_mount(&fs, drv, 0);
    REQUIRE(fr_result == FR_OK);

    // Open, write and read data
    fr_result = f_open(&file, "0:/test.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
    REQUIRE(fr_result == FR_OK);

    // Generate data
    uint32_t data_size = 1000;

    char *data = (char *) malloc(data_size);
    char *read = (char *) malloc(data_size);

    for (uint32_t i = 0; i < data_size; i += sizeof(i)) {
        *((uint32_t *)(data + i)) = i;
    }

    // Write generated data
    fr_result = f_write(&file, data, data_size, &bw);
    REQUIRE(fr_result == FR_OK);
    REQUIRE(bw == data_size);

    // Move to beginning of file
    fr_result = f_lseek(&file, 0);
    REQUIRE(fr_result == FR_OK);

    // Read written data
    fr_result = f_read(&file, read, data_size, &bw);
    REQUIRE(fr_result == FR_OK);
    REQUIRE(bw == data_size);

    REQUIRE(memcmp(data, read, data_size) == 0);

    // Close file
    fr_result = f_close(&file);
    REQUIRE(fr_result == FR_OK);

    // Unmount default volume
    fr_result = f_mount(0, drv, 0);
    REQUIRE(fr_result == FR_OK);

    // Clear
    free(read);
    free(data);
    ff_diskio_unregister(pdrv);
    ff_diskio_clear_pdrv_nand(device_handle);
    spi_nand_flash_deinit_device(device_handle);
    esp_partition_unload_all();
}

TEST_CASE("verify mark_bad_block works", "[spi_nand_flash]")
{
    spi_nand_flash_config_t nand_flash_config;
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
    esp_partition_unload_all();
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
    spi_nand_flash_config_t nand_flash_config;
    spi_nand_flash_device_t *device_handle;
    REQUIRE(spi_nand_flash_init_device(&nand_flash_config, &device_handle) == ESP_OK);

    uint32_t sector_num, sector_size, block_size;
    REQUIRE(spi_nand_flash_get_capacity(device_handle, &sector_num) == 0);
    REQUIRE(spi_nand_flash_get_sector_size(device_handle, &sector_size) == 0);
    REQUIRE(spi_nand_flash_get_block_size(device_handle, &block_size) == 0);

    uint8_t *pattern_buf = (uint8_t *)heap_caps_malloc(sector_size, MALLOC_CAP_DEFAULT);
    REQUIRE(pattern_buf != NULL);
    uint8_t *temp_buf = (uint8_t *)heap_caps_malloc(sector_size, MALLOC_CAP_DEFAULT);
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
    esp_partition_unload_all();
}
