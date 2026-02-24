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
#include "esp_blockdev.h"
#include "esp_nand_blockdev.h"
#include "spi_nand_flash_test_helpers.h"


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

static void do_single_write_test(esp_blockdev_handle_t bdl, uint32_t start_page, uint16_t page_count);
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

static void setup_nand_flash(spi_device_handle_t *spi_handle, spi_nand_flash_io_mode_t mode, uint8_t flags, esp_blockdev_handle_t *bdl_handle)
{
    spi_device_handle_t spi;
    setup_chip(&spi, flags);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = flags,
        .io_mode = mode,
    };
    esp_blockdev_handle_t wl_bdl;
    TEST_ESP_OK(spi_nand_flash_init_with_layers(&nand_flash_config, &wl_bdl));

    *spi_handle = spi;
    *bdl_handle = wl_bdl;
}

static void deinit_nand_flash(spi_device_handle_t spi, esp_blockdev_handle_t bdl_handle)
{
    bdl_handle->ops->release(bdl_handle);
    spi_bus_remove_device(spi);
    spi_bus_free(HOST_ID);
}

TEST_CASE("erase nand flash using block device interface [via dhara]", "[spi_nand_flash]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t bdl_handle;
    setup_nand_flash(&spi, SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX, &bdl_handle);

    TEST_ESP_OK(bdl_handle->ops->erase(bdl_handle, 0, bdl_handle->geometry.disk_size));

    do_single_write_test(bdl_handle, 1, 1);
    deinit_nand_flash(spi, bdl_handle);
}

static void do_single_write_test(esp_blockdev_handle_t bdl, uint32_t start_page, uint16_t page_count)
{
    uint8_t *temp_buf = NULL;
    uint8_t *pattern_buf = NULL;
    uint32_t page_size = bdl->geometry.write_size;

    uint32_t num_pages;
    bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_AVAILABLE_SECTORS, &num_pages);

    TEST_ESP_OK((start_page + page_count) > num_pages);

    pattern_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(pattern_buf);
    temp_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(temp_buf);

    spi_nand_flash_fill_buffer(pattern_buf, page_size / sizeof(uint32_t));

    int64_t read_time = 0;
    int64_t write_time = 0;

    for (uint32_t i = start_page; i < (start_page + page_count); i++) {
        int64_t start = esp_timer_get_time();
        bdl->ops->write(bdl, pattern_buf, i * page_size, page_size);
        write_time += esp_timer_get_time() - start;

        memset((void *)temp_buf, 0x00, page_size);

        start = esp_timer_get_time();
        bdl->ops->read(bdl, temp_buf, page_size, i * page_size, page_size);
        read_time += esp_timer_get_time() - start;

        TEST_ASSERT_EQUAL(0, spi_nand_flash_check_buffer(temp_buf, page_size / sizeof(uint32_t)));
    }
    free(pattern_buf);
    free(temp_buf);

    printf("Wrote %" PRIu32 " bytes in %" PRId64 " us, avg %.2f kB/s\n", page_size * page_count, write_time, (float)page_size * page_count / write_time * 1000);
    printf("Read %" PRIu32 " bytes in %" PRId64 " us, avg %.2f kB/s\n", page_size * page_count, read_time, (float)page_size * page_count / read_time * 1000);
}

static void test_write_nand_flash_pages(spi_nand_flash_io_mode_t mode, uint8_t flags)
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t bdl_handle;
    setup_nand_flash(&spi, mode, flags, &bdl_handle);

    uint32_t page_size = bdl_handle->geometry.write_size;
    uint32_t num_pages;
    bdl_handle->ops->ioctl(bdl_handle, ESP_BLOCKDEV_CMD_GET_AVAILABLE_SECTORS, &num_pages);
    printf("Number of pages: %" PRIu32 ", Page size: %" PRIu32 "\n", num_pages, page_size);

    do_single_write_test(bdl_handle, 1, 16);
    do_single_write_test(bdl_handle, 16, 32);
    do_single_write_test(bdl_handle, 32, 64);
    do_single_write_test(bdl_handle, 64, 128);
    do_single_write_test(bdl_handle, num_pages / 2, 32) ;
    do_single_write_test(bdl_handle, num_pages / 2, 256);
    do_single_write_test(bdl_handle, num_pages - 20, 16);

    deinit_nand_flash(spi, bdl_handle);
}

TEST_CASE("read and write nand flash pages using block device interface (via dhara) (sio half-duplex)", "[spi_nand_flash]")
{
    test_write_nand_flash_pages(SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX);
}

static void test_nand_operations(spi_nand_flash_io_mode_t mode, uint8_t flags)
{

    spi_device_handle_t spi;
    setup_chip(&spi, flags);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = flags,
        .io_mode = mode,
    };
    esp_blockdev_handle_t bdl_handle;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl_handle));

    uint32_t page_size = bdl_handle->geometry.write_size;
    uint32_t block_size = bdl_handle->geometry.erase_size;
    uint32_t num_pages = bdl_handle->geometry.disk_size / bdl_handle->geometry.read_size;

    printf("Number of pages: %" PRIu32 ", Page size: %" PRIu32 "\n", num_pages, page_size);

    uint8_t *pattern_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(pattern_buf);
    uint8_t *temp_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(temp_buf);

    spi_nand_flash_fill_buffer(pattern_buf, page_size / sizeof(uint32_t));

    uint32_t src_block = 20;
    uint32_t test_page = src_block * (block_size / page_size); // pages_per_block
    TEST_ESP_OK(bdl_handle->ops->erase(bdl_handle, src_block * block_size, block_size));
    TEST_ASSERT_TRUE(test_page < num_pages);

    // Verify if test_page is free
    esp_blockdev_cmd_arg_is_free_page_t page_free_status = {test_page, true};
    TEST_ESP_OK(bdl_handle->ops->ioctl(bdl_handle, ESP_BLOCKDEV_CMD_IS_FREE_PAGE, &page_free_status));
    TEST_ASSERT_TRUE(page_free_status.status == true);
    // Write/program test_page
    TEST_ESP_OK(bdl_handle->ops->write(bdl_handle, pattern_buf, test_page * page_size, page_size));
    // Verify if test_page is used/programmed
    TEST_ESP_OK(bdl_handle->ops->ioctl(bdl_handle, ESP_BLOCKDEV_CMD_IS_FREE_PAGE, &page_free_status));
    TEST_ASSERT_TRUE(page_free_status.status == false);
    // read test_page and verify with pattern_buf
    TEST_ESP_OK(bdl_handle->ops->read(bdl_handle, temp_buf, page_size, test_page * page_size, page_size));
    TEST_ASSERT_EQUAL(0, spi_nand_flash_check_buffer(temp_buf, page_size / sizeof(uint32_t)));

    free(pattern_buf);
    free(temp_buf);
    deinit_nand_flash(spi, bdl_handle);
}

TEST_CASE("verify nand_prog, nand_read, nand_is_free works (bypassing dhara) using block device interface (sio half-duplex)", "[spi_nand_flash]")
{
    test_nand_operations(SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX);
}

TEST_CASE("verify mark_bad_block works with bdl interface", "[spi_nand_flash]")
{
    spi_nand_flash_io_mode_t mode = SPI_NAND_IO_MODE_SIO;
    uint8_t flags = SPI_DEVICE_HALFDUPLEX;

    spi_device_handle_t spi;
    setup_chip(&spi, flags);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = flags,
        .io_mode = mode,
    };
    esp_blockdev_handle_t nand_bdl;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &nand_bdl));

    uint32_t block_size = nand_bdl->geometry.erase_size;
    uint32_t block_num = nand_bdl->geometry.disk_size / block_size;

    uint32_t test_block = 15;
    TEST_ASSERT_TRUE(test_block < block_num);
    TEST_ESP_OK(nand_bdl->ops->erase(nand_bdl, test_block * block_size, block_size));
    // Verify if test_block is not bad block
    esp_blockdev_cmd_arg_is_bad_block_t bad_block_status = {test_block, false};
    TEST_ESP_OK(nand_bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_IS_BAD_BLOCK, &bad_block_status));
    TEST_ASSERT_TRUE(bad_block_status.status == false);
    // mark test_block as a bad block
    TEST_ESP_OK(nand_bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_MARK_BAD_BLOCK, &test_block));
    // Verify if test_block is marked as bad block
    TEST_ESP_OK(nand_bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_IS_BAD_BLOCK, &bad_block_status));
    TEST_ASSERT_TRUE(bad_block_status.status == true);

    deinit_nand_flash(spi, nand_bdl);
}

TEST_CASE("verify ioctl (bad blocks and ecc stats) works with bdl interface", "[spi_nand_flash]")
{
    spi_nand_flash_io_mode_t mode = SPI_NAND_IO_MODE_SIO;
    uint8_t flags = SPI_DEVICE_HALFDUPLEX;

    spi_device_handle_t spi;
    setup_chip(&spi, flags);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = flags,
        .io_mode = mode,
    };
    esp_blockdev_handle_t nand_bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &nand_bdl));
    TEST_ASSERT_TRUE(nand_bdl != NULL);

    uint32_t bad_block_count = 0xFFFF;
    TEST_ESP_OK(nand_bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_GET_BAD_BLOCKS_COUNT, &bad_block_count));
    TEST_ASSERT_TRUE(bad_block_count != 0xFFFF);

    esp_blockdev_cmd_arg_ecc_stats_t ecc_stats;
    memset(&ecc_stats, 0xFF, sizeof(ecc_stats));
    TEST_ESP_OK(nand_bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_GET_ECC_STATS, &ecc_stats));
    TEST_ASSERT_TRUE(ecc_stats.ecc_threshold != 0xFF);


    deinit_nand_flash(spi, nand_bdl);
}
