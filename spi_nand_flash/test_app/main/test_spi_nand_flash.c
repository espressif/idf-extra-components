/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <inttypes.h>
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "spi_nand_flash.h"
#include "nand_private/nand_impl_wrap.h"
#include "unity.h"
#include "soc/spi_pins.h"
#include "sdkconfig.h"


// Pin mapping
// ESP32 (VSPI)
#ifdef CONFIG_IDF_TARGET_ESP32
#define HOST_ID     SPI3_HOST
#define PIN_MOSI     SPI3_IOMUX_PIN_NUM_MOSI
#define PIN_MISO     SPI3_IOMUX_PIN_NUM_MISO
#define PIN_CLK      SPI3_IOMUX_PIN_NUM_CLK
#define PIN_CS       SPI3_IOMUX_PIN_NUM_CS
#define PIN_WP       SPI3_IOMUX_PIN_NUM_WP
#define PIN_HD       SPI3_IOMUX_PIN_NUM_HD
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#else // Other chips (SPI2/HSPI)
#define HOST_ID      SPI2_HOST
#define PIN_MOSI     SPI2_IOMUX_PIN_NUM_MOSI
#define PIN_MISO     SPI2_IOMUX_PIN_NUM_MISO
#define PIN_CLK      SPI2_IOMUX_PIN_NUM_CLK
#define PIN_CS       SPI2_IOMUX_PIN_NUM_CS
#define PIN_WP       SPI2_IOMUX_PIN_NUM_WP
#define PIN_HD       SPI2_IOMUX_PIN_NUM_HD
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#endif

#define PATTERN_SEED    0x12345678

static void do_single_write_test(spi_nand_flash_device_t *flash, uint32_t start_sec, uint16_t sec_count);
static void setup_bus(spi_host_device_t host_id)
{
    spi_bus_config_t spi_bus_cfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadhd_io_num = PIN_HD,
        .quadwp_io_num = PIN_WP,
        .max_transfer_sz = 64,
    };
    esp_err_t ret = spi_bus_initialize(host_id, &spi_bus_cfg, SPI_DMA_CHAN);
    TEST_ESP_OK(ret);
}

static void setup_chip(spi_device_handle_t *spi, uint8_t flags)
{
    setup_bus(HOST_ID);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 10,
        .flags = flags,
    };

    TEST_ESP_OK(spi_bus_add_device(HOST_ID, &devcfg, spi));
}

static void setup_nand_flash(spi_nand_flash_device_t **out_handle, spi_device_handle_t *spi_handle, spi_nand_flash_io_mode_t mode, uint8_t flags)
{
    spi_device_handle_t spi;
    setup_chip(&spi, flags);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = flags,
        .io_mode = mode,
    };
    spi_nand_flash_device_t *device_handle;
    TEST_ESP_OK(spi_nand_flash_init_device(&nand_flash_config, &device_handle));

    *out_handle = device_handle;
    *spi_handle = spi;
}

static void deinit_nand_flash(spi_nand_flash_device_t *flash, spi_device_handle_t spi)
{
    spi_nand_flash_deinit_device(flash);
    spi_bus_remove_device(spi);
    spi_bus_free(HOST_ID);
}

TEST_CASE("erase nand flash", "[spi_nand_flash]")
{
    spi_nand_flash_device_t *nand_flash_device_handle;
    spi_device_handle_t spi;
    setup_nand_flash(&nand_flash_device_handle, &spi, SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX);
    TEST_ESP_OK(spi_nand_erase_chip(nand_flash_device_handle));
    do_single_write_test(nand_flash_device_handle, 1, 1);
    deinit_nand_flash(nand_flash_device_handle, spi);
}

TEST_CASE("verify mark_bad_block works", "[spi_nand_flash]")
{
    spi_nand_flash_device_t *nand_flash_device_handle;
    spi_device_handle_t spi;
    setup_nand_flash(&nand_flash_device_handle, &spi, SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX);
    uint32_t sector_num, sector_size;

    TEST_ESP_OK(spi_nand_flash_get_capacity(nand_flash_device_handle, &sector_num));
    TEST_ESP_OK(spi_nand_flash_get_sector_size(nand_flash_device_handle, &sector_size));
    printf("Number of sectors: %" PRIu32 ", Sector size: %" PRIu32 "\n", sector_num, sector_size);

    uint32_t test_block = 15;
    if (test_block < sector_num) {
        bool is_bad_status = false;
        // Verify if test_block is not bad block
        TEST_ESP_OK(nand_wrap_is_bad(nand_flash_device_handle, test_block, &is_bad_status));
        TEST_ASSERT_TRUE(is_bad_status == false);
        // mark test_block as a bad block
        TEST_ESP_OK(nand_wrap_mark_bad(nand_flash_device_handle, test_block));
        // Verify if test_block is marked as bad block
        TEST_ESP_OK(nand_wrap_is_bad(nand_flash_device_handle, test_block, &is_bad_status));
        TEST_ASSERT_TRUE(is_bad_status == true);
    }

    deinit_nand_flash(nand_flash_device_handle, spi);
}

static void check_buffer(uint32_t seed, const uint8_t *src, size_t count)
{
    srand(seed);
    for (size_t i = 0; i < count; ++i) {
        uint32_t val;
        memcpy(&val, src + i * sizeof(uint32_t), sizeof(val));
        TEST_ASSERT_EQUAL_HEX32(rand(), val);
    }
}

static void fill_buffer(uint32_t seed, uint8_t *dst, size_t count)
{
    srand(seed);
    for (size_t i = 0; i < count; ++i) {
        uint32_t val = rand();
        memcpy(dst + i * sizeof(uint32_t), &val, sizeof(val));
    }
}

static void do_single_write_test(spi_nand_flash_device_t *flash, uint32_t start_sec, uint16_t sec_count)
{
    uint8_t *temp_buf = NULL;
    uint8_t *pattern_buf = NULL;
    uint32_t sector_size, sector_num;

    TEST_ESP_OK(spi_nand_flash_get_capacity(flash, &sector_num));
    TEST_ESP_OK(spi_nand_flash_get_sector_size(flash, &sector_size));

    TEST_ESP_OK((start_sec + sec_count) > sector_num);

    pattern_buf = (uint8_t *)heap_caps_malloc(sector_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(pattern_buf);
    temp_buf = (uint8_t *)heap_caps_malloc(sector_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(temp_buf);

    fill_buffer(PATTERN_SEED, pattern_buf, sector_size / sizeof(uint32_t));

    int64_t read_time = 0;
    int64_t write_time = 0;

    for (int i = start_sec; i < (start_sec + sec_count); i++) {
        int64_t start = esp_timer_get_time();
        TEST_ESP_OK(spi_nand_flash_write_sector(flash, pattern_buf, i));
        write_time += esp_timer_get_time() - start;

        memset((void *)temp_buf, 0x00, sector_size);

        start = esp_timer_get_time();
        TEST_ESP_OK(spi_nand_flash_read_sector(flash, temp_buf, i));
        read_time += esp_timer_get_time() - start;

        check_buffer(PATTERN_SEED, temp_buf, sector_size / sizeof(uint32_t));
    }
    free(pattern_buf);
    free(temp_buf);

    printf("Wrote %" PRIu32 " bytes in %" PRId64 " us, avg %.2f kB/s\n", sector_size * sec_count, write_time, (float)sector_size * sec_count / write_time * 1000);
    printf("Read %" PRIu32 " bytes in %" PRId64 " us, avg %.2f kB/s\n", sector_size * sec_count, read_time, (float)sector_size * sec_count / read_time * 1000);
}

static void test_write_nand_flash_sectors(spi_nand_flash_io_mode_t mode, uint8_t flags)
{
    uint32_t sector_num, sector_size;
    spi_nand_flash_device_t *nand_flash_device_handle;
    spi_device_handle_t spi;
    setup_nand_flash(&nand_flash_device_handle, &spi, mode, flags);

    TEST_ESP_OK(spi_nand_flash_get_capacity(nand_flash_device_handle, &sector_num));
    TEST_ESP_OK(spi_nand_flash_get_sector_size(nand_flash_device_handle, &sector_size));
    printf("Number of sectors: %" PRIu32 ", Sector size: %" PRIu32 "\n", sector_num, sector_size);

    do_single_write_test(nand_flash_device_handle, 1, 16);
    do_single_write_test(nand_flash_device_handle, 16, 32);
    do_single_write_test(nand_flash_device_handle, 32, 64);
    do_single_write_test(nand_flash_device_handle, 64, 128);
    do_single_write_test(nand_flash_device_handle, sector_num / 2, 32);
    do_single_write_test(nand_flash_device_handle, sector_num / 2, 256);
    do_single_write_test(nand_flash_device_handle, sector_num - 20, 16);

    deinit_nand_flash(nand_flash_device_handle, spi);
}

TEST_CASE("read and write nand flash sectors (sio half-duplex)", "[spi_nand_flash]")
{
    test_write_nand_flash_sectors(SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX);
}

TEST_CASE("read and write nand flash sectors (sio full-duplex)", "[spi_nand_flash]")
{
    test_write_nand_flash_sectors(SPI_NAND_IO_MODE_SIO, 0); //set flags 0 for full-duplex mode
}

TEST_CASE("read and write nand flash sectors (dio)", "[spi_nand_flash]")
{
    test_write_nand_flash_sectors(SPI_NAND_IO_MODE_DIO, SPI_DEVICE_HALFDUPLEX);
}

TEST_CASE("read and write nand flash sectors (dout)", "[spi_nand_flash]")
{
    test_write_nand_flash_sectors(SPI_NAND_IO_MODE_DOUT, SPI_DEVICE_HALFDUPLEX);
}

static void test_copy_nand_flash_sectors(spi_nand_flash_io_mode_t mode, uint8_t flags)
{
    spi_nand_flash_device_t *nand_flash_device_handle;
    spi_device_handle_t spi;
    setup_nand_flash(&nand_flash_device_handle, &spi, mode, flags);
    uint32_t sector_num, sector_size;
    uint32_t src_sec = 10;
    uint32_t dst_sec = 11;

    TEST_ESP_OK(spi_nand_flash_get_capacity(nand_flash_device_handle, &sector_num));
    TEST_ESP_OK(spi_nand_flash_get_sector_size(nand_flash_device_handle, &sector_size));
    printf("Number of sectors: %" PRIu32 ", Sector size: %" PRIu32 "\n", sector_num, sector_size);

    if (src_sec < sector_num && dst_sec < sector_num) {
        do_single_write_test(nand_flash_device_handle, src_sec, 1);
        TEST_ESP_OK(spi_nand_flash_copy_sector(nand_flash_device_handle, src_sec, dst_sec));
    }
    deinit_nand_flash(nand_flash_device_handle, spi);
}

TEST_CASE("copy nand flash sectors (sio half-duplex)", "[spi_nand_flash]")
{
    test_copy_nand_flash_sectors(SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX);
}

TEST_CASE("copy nand flash sectors (sio full-duplex)", "[spi_nand_flash]")
{
    test_copy_nand_flash_sectors(SPI_NAND_IO_MODE_SIO, 0);
}

TEST_CASE("copy nand flash sectors (dio)", "[spi_nand_flash]")
{
    test_copy_nand_flash_sectors(SPI_NAND_IO_MODE_DIO, SPI_DEVICE_HALFDUPLEX);
}

TEST_CASE("copy nand flash sectors (dout)", "[spi_nand_flash]")
{
    test_copy_nand_flash_sectors(SPI_NAND_IO_MODE_DOUT, SPI_DEVICE_HALFDUPLEX);
}

static void test_nand_operations(spi_nand_flash_io_mode_t mode, uint8_t flags)
{
    spi_nand_flash_device_t *nand_flash_device_handle;
    spi_device_handle_t spi;
    uint32_t sector_num, sector_size, block_size;

    setup_nand_flash(&nand_flash_device_handle, &spi, SPI_NAND_IO_MODE_SIO, flags);

    TEST_ESP_OK(spi_nand_flash_get_capacity(nand_flash_device_handle, &sector_num));
    TEST_ESP_OK(spi_nand_flash_get_sector_size(nand_flash_device_handle, &sector_size));
    TEST_ESP_OK(spi_nand_flash_get_block_size(nand_flash_device_handle, &block_size));
    printf("Number of sectors: %" PRIu32 ", Sector size: %" PRIu32 "\n", sector_num, sector_size);

    uint8_t *pattern_buf = (uint8_t *)heap_caps_malloc(sector_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(pattern_buf);
    uint8_t *temp_buf = (uint8_t *)heap_caps_malloc(sector_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(temp_buf);

    fill_buffer(PATTERN_SEED, pattern_buf, sector_size / sizeof(uint32_t));

    bool is_page_free = true;
    uint32_t test_block = 20;
    uint32_t test_page = test_block * (block_size / sector_size); //(block_num * pages_per_block)
    uint32_t dst_page = test_page + 1;
    TEST_ESP_OK(nand_wrap_erase_block(nand_flash_device_handle, test_block));
    if (test_page < sector_num) {
        // Verify if test_page is free
        TEST_ESP_OK(nand_wrap_is_free(nand_flash_device_handle, test_page, &is_page_free));
        TEST_ASSERT_TRUE(is_page_free == true);
        // Write/program test_page
        TEST_ESP_OK(nand_wrap_prog(nand_flash_device_handle, test_page, pattern_buf));
        // Verify if test_page is used/programmed
        TEST_ESP_OK(nand_wrap_is_free(nand_flash_device_handle, test_page, &is_page_free));
        TEST_ASSERT_TRUE(is_page_free == false);
        // read test_page and verify with pattern_buf
        TEST_ESP_OK(nand_wrap_read(nand_flash_device_handle, test_page, 0, sector_size, temp_buf));
        check_buffer(PATTERN_SEED, temp_buf, sector_size / sizeof(uint32_t));
        // Copy test_page to dst_page
        TEST_ESP_OK(nand_wrap_copy(nand_flash_device_handle, test_page, dst_page));
        // read dst_page and verify with pattern_buf
        TEST_ESP_OK(nand_wrap_read(nand_flash_device_handle, dst_page, 0, sector_size, temp_buf));
        check_buffer(PATTERN_SEED, temp_buf, sector_size / sizeof(uint32_t));
    }
    free(pattern_buf);
    free(temp_buf);
    deinit_nand_flash(nand_flash_device_handle, spi);
}

TEST_CASE("verify nand_prog, nand_read, nand_copy, nand_is_free works (bypassing dhara) (sio half-duplex)", "[spi_nand_flash]")
{
    test_nand_operations(SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX);
}

TEST_CASE("verify nand_prog, nand_read, nand_copy, nand_is_free works (bypassing dhara) (sio full-duplex)", "[spi_nand_flash]")
{
    test_nand_operations(SPI_NAND_IO_MODE_SIO, 0);
}

TEST_CASE("verify nand_prog, nand_read, nand_copy, nand_is_free works (bypassing dhara) (dio)", "[spi_nand_flash]")
{
    test_nand_operations(SPI_NAND_IO_MODE_DIO, SPI_DEVICE_HALFDUPLEX);
}

TEST_CASE("verify nand_prog, nand_read, nand_copy, nand_is_free works (bypassing dhara) (dout)", "[spi_nand_flash]")
{
    test_nand_operations(SPI_NAND_IO_MODE_DOUT, SPI_DEVICE_HALFDUPLEX);
}

TEST_CASE("Fail safe test if chip is not detected", "[spi_nand_flash]")
{
    spi_device_handle_t spi;
    spi_bus_config_t spi_bus_cfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_MISO,        // Initialize with wrong pin
        .max_transfer_sz = 64,
    };
    esp_err_t ret = spi_bus_initialize(HOST_ID, &spi_bus_cfg, SPI_DMA_CHAN);
    TEST_ESP_OK(ret);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 10,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    TEST_ESP_OK(spi_bus_add_device(HOST_ID, &devcfg, &spi));

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    spi_nand_flash_device_t *device_handle;
    esp_err_t err = spi_nand_flash_init_device(&nand_flash_config, &device_handle);
    TEST_ASSERT_TRUE(err != ESP_OK);

    spi_bus_remove_device(spi);
    spi_bus_free(HOST_ID);
}
