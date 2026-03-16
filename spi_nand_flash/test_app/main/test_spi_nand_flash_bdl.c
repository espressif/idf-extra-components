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

    /* Erase length must be aligned to erase_size (block size) */
    size_t erase_size = (size_t)bdl_handle->geometry.erase_size;
    size_t erase_len = (size_t)((bdl_handle->geometry.disk_size / erase_size) * erase_size);
    TEST_ESP_OK(bdl_handle->ops->erase(bdl_handle, 0, erase_len));

    do_single_write_test(bdl_handle, 1, 1);
    deinit_nand_flash(spi, bdl_handle);
}

static void do_single_write_test(esp_blockdev_handle_t bdl, uint32_t start_page, uint16_t page_count)
{
    uint8_t *temp_buf = NULL;
    uint8_t *pattern_buf = NULL;
    uint32_t page_size = bdl->geometry.write_size;

    uint32_t num_pages = 0;
    bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_AVAILABLE_SECTORS, &num_pages);

    TEST_ASSERT_TRUE((start_page + page_count) <= num_pages);

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

/* Returns 0 on success, non-zero on failure. Frees buffers so caller can always run cleanup. */
static int do_multiple_page_write_test(esp_blockdev_handle_t bdl, uint32_t start_page, uint16_t page_count)
{
    uint8_t *temp_buf = NULL;
    uint8_t *pattern_buf = NULL;
    uint32_t page_size = bdl->geometry.write_size;
    uint32_t num_pages = bdl->geometry.disk_size / page_size;
    int ret = -1;

    if ((start_page + page_count) > num_pages) {
        return -1;
    }

    pattern_buf = (uint8_t *)heap_caps_malloc(page_size * page_count, MALLOC_CAP_DEFAULT);
    if (pattern_buf == NULL) {
        return -1;
    }
    temp_buf = (uint8_t *)heap_caps_malloc(page_size * page_count, MALLOC_CAP_DEFAULT);
    if (temp_buf == NULL) {
        free(pattern_buf);
        return -1;
    }

    spi_nand_flash_fill_buffer(pattern_buf, page_size * page_count / sizeof(uint32_t));

    int64_t read_time = 0;
    int64_t write_time = 0;

    int64_t start = esp_timer_get_time();
    if (bdl->ops->write(bdl, pattern_buf, start_page * page_size, page_count * page_size) != ESP_OK) {
        free(pattern_buf);
        free(temp_buf);
        return -1;
    }
    write_time += esp_timer_get_time() - start;

    memset((void *)temp_buf, 0x00, page_count * page_size);

    start = esp_timer_get_time();
    if (bdl->ops->read(bdl, temp_buf, page_count * page_size, start_page * page_size, page_count * page_size) != ESP_OK) {
        free(pattern_buf);
        free(temp_buf);
        return -1;
    }
    read_time += esp_timer_get_time() - start;

    ret = spi_nand_flash_check_buffer(temp_buf, page_size * page_count / sizeof(uint32_t));
    free(pattern_buf);
    free(temp_buf);

    printf("Wrote %" PRIu32 " bytes in %" PRId64 " us, avg %.2f kB/s\n", page_size * page_count, write_time, (float)page_size * page_count / write_time * 1000);
    printf("Read %" PRIu32 " bytes in %" PRId64 " us, avg %.2f kB/s\n", page_size * page_count, read_time, (float)page_size * page_count / read_time * 1000);
    return ret;
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

    int ret = do_multiple_page_write_test(bdl_handle, 1, 2);
    do_single_write_test(bdl_handle, 16, 32);
    deinit_nand_flash(spi, bdl_handle);
    TEST_ASSERT_EQUAL(0, ret);
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

    uint32_t src_block = 20;
    uint32_t test_page = src_block * (block_size / page_size); // pages_per_block
    uint32_t page_count = 2;
    TEST_ESP_OK(bdl_handle->ops->erase(bdl_handle, src_block * block_size, block_size));
    TEST_ASSERT_TRUE(test_page < num_pages);

    // Verify if test_page is free
    for (uint32_t page = test_page; page < (test_page + page_count); page++) {
        esp_blockdev_cmd_arg_is_free_page_t page_free_status = {page, false};
        TEST_ESP_OK(bdl_handle->ops->ioctl(bdl_handle, ESP_BLOCKDEV_CMD_IS_FREE_PAGE, &page_free_status));
        TEST_ASSERT_TRUE(page_free_status.status == true);
    }

    TEST_ASSERT_EQUAL(0, do_multiple_page_write_test(bdl_handle, test_page, page_count));

    // Verify if test_page is free
    for (uint32_t page = test_page; page < (test_page + page_count); page++) {
        esp_blockdev_cmd_arg_is_free_page_t page_free_status = {page, true};
        TEST_ESP_OK(bdl_handle->ops->ioctl(bdl_handle, ESP_BLOCKDEV_CMD_IS_FREE_PAGE, &page_free_status));
        TEST_ASSERT_TRUE(page_free_status.status == false);
    }

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

TEST_CASE("Flash BDL geometry and device_flags after nand_flash_get_blockdev", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    uint32_t block_size = bdl->geometry.erase_size;
    uint32_t num_blocks = (uint32_t)(bdl->geometry.disk_size / block_size);
    TEST_ASSERT_EQUAL_UINT32(num_blocks * block_size, (uint32_t)bdl->geometry.disk_size);

    TEST_ASSERT_EQUAL(bdl->geometry.read_size, bdl->geometry.write_size);
    TEST_ASSERT_TRUE(bdl->geometry.read_size > 0);
    TEST_ASSERT_TRUE(bdl->geometry.erase_size > 0);

    TEST_ASSERT_TRUE(bdl->device_flags.erase_before_write);
    TEST_ASSERT_TRUE(bdl->device_flags.and_type_write);
    TEST_ASSERT_TRUE(bdl->device_flags.default_val_after_erase);

    TEST_ASSERT_NOT_NULL(bdl->ops);
    TEST_ASSERT_NOT_NULL(bdl->ops->read);
    TEST_ASSERT_NOT_NULL(bdl->ops->write);
    TEST_ASSERT_NOT_NULL(bdl->ops->erase);
    TEST_ASSERT_NOT_NULL(bdl->ops->ioctl);
    TEST_ASSERT_NOT_NULL(bdl->ops->release);

    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL multi-block erase honours erase_len", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    uint32_t block_size = bdl->geometry.erase_size;
    uint32_t page_size = bdl->geometry.write_size;
    const uint32_t num_blocks_to_erase = 3;
    const uint32_t start_block = 10;
    uint64_t start_addr = (uint64_t)start_block * block_size;
    size_t erase_len = num_blocks_to_erase * block_size;

    TEST_ESP_OK(bdl->ops->erase(bdl, start_addr, erase_len));

    uint8_t *read_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(read_buf);
    for (uint32_t blk = 0; blk < num_blocks_to_erase; blk++) {
        uint32_t pages_per_block = block_size / page_size;
        for (uint32_t p = 0; p < pages_per_block; p++) {
            uint64_t addr = start_addr + (uint64_t)(blk * block_size + p * page_size);
            TEST_ESP_OK(bdl->ops->read(bdl, read_buf, page_size, addr, page_size));
            for (size_t i = 0; i < page_size; i++) {
                TEST_ASSERT_EQUAL_HEX8(0xFF, read_buf[i]);
            }
        }
    }
    free(read_buf);
    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL GET_NAND_FLASH_INFO ioctl", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    esp_blockdev_cmd_arg_nand_flash_info_t flash_info;
    memset(&flash_info, 0, sizeof(flash_info));
    TEST_ESP_OK(bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_NAND_FLASH_INFO, &flash_info));

    TEST_ASSERT_TRUE(flash_info.device_info.manufacturer_id != 0 || flash_info.device_info.device_id != 0);
    TEST_ASSERT_TRUE(strnlen((char *)flash_info.device_info.chip_name, sizeof(flash_info.device_info.chip_name)) > 0);
    TEST_ASSERT_EQUAL(bdl->geometry.read_size, flash_info.geometry.page_size);
    TEST_ASSERT_EQUAL(bdl->geometry.erase_size, flash_info.geometry.block_size);
    TEST_ASSERT_EQUAL((uint32_t)(bdl->geometry.disk_size / bdl->geometry.erase_size), flash_info.geometry.num_blocks);

    deinit_nand_flash(spi, bdl);
}

TEST_CASE("nand_flash_get_blockdev and spi_nand_flash_wl_get_blockdev error paths", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);
    spi_nand_flash_config_t config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t out = NULL;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, nand_flash_get_blockdev(NULL, &out));
    TEST_ASSERT_NULL(out);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, nand_flash_get_blockdev(&config, NULL));

    esp_blockdev_handle_t flash_bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&config, &flash_bdl));
    TEST_ASSERT_NOT_NULL(flash_bdl);

    out = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, spi_nand_flash_wl_get_blockdev(NULL, &out));
    TEST_ASSERT_NULL(out);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, spi_nand_flash_wl_get_blockdev(flash_bdl, NULL));

    deinit_nand_flash(spi, flash_bdl);
}

TEST_CASE("Flash BDL erase invalid args", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    uint32_t block_size = bdl->geometry.erase_size;
    esp_err_t ret = bdl->ops->erase(bdl, block_size / 2, block_size);
    TEST_ASSERT_TRUE(ret == ESP_ERR_INVALID_ARG || ret == ESP_ERR_INVALID_SIZE);

    ret = bdl->ops->erase(bdl, 0, block_size / 2);
    TEST_ASSERT_TRUE(ret == ESP_ERR_INVALID_ARG || ret == ESP_ERR_INVALID_SIZE);

    ret = bdl->ops->erase(bdl, 0, 0);
    TEST_ASSERT_TRUE(ret == ESP_ERR_INVALID_ARG || ret == ESP_ERR_INVALID_SIZE);

    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL unsupported ioctl returns ESP_ERR_NOT_SUPPORTED", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    uint32_t dummy = 0;
    esp_err_t ret = bdl->ops->ioctl(bdl, (uint8_t)0xFF, &dummy);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, ret);

    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL COPY_PAGE ioctl", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    uint32_t page_size = bdl->geometry.write_size;
    uint32_t block_size = bdl->geometry.erase_size;
    uint32_t num_pages = (uint32_t)(bdl->geometry.disk_size / page_size);
    uint32_t pages_per_block = block_size / page_size;

    uint32_t src_block = 5;
    uint32_t dst_block = 6;
    uint32_t src_page = src_block * pages_per_block;
    uint32_t dst_page = dst_block * pages_per_block;
    TEST_ASSERT_TRUE(dst_page < num_pages);

    TEST_ESP_OK(bdl->ops->erase(bdl, src_block * block_size, block_size));
    TEST_ESP_OK(bdl->ops->erase(bdl, dst_block * block_size, block_size));

    uint8_t *pattern_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    uint8_t *read_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(pattern_buf);
    TEST_ASSERT_NOT_NULL(read_buf);

    spi_nand_flash_fill_buffer(pattern_buf, page_size / sizeof(uint32_t));
    TEST_ESP_OK(bdl->ops->write(bdl, pattern_buf, src_page * (uint64_t)page_size, page_size));

    esp_blockdev_cmd_arg_copy_page_t copy_cmd = {
        .src_page = src_page,
        .dst_page = dst_page,
    };
    TEST_ESP_OK(bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_COPY_PAGE, &copy_cmd));

    memset(read_buf, 0, page_size);
    TEST_ESP_OK(bdl->ops->read(bdl, read_buf, page_size, dst_page * (uint64_t)page_size, page_size));
    TEST_ASSERT_EQUAL(0, spi_nand_flash_check_buffer(read_buf, page_size / sizeof(uint32_t)));

    free(pattern_buf);
    free(read_buf);
    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL GET_PAGE_ECC_STATUS ioctl", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    uint32_t page_size = bdl->geometry.write_size;
    uint32_t block_size = bdl->geometry.erase_size;
    uint32_t test_page = (bdl->geometry.disk_size / page_size) / 2;

    TEST_ESP_OK(bdl->ops->erase(bdl, (test_page * page_size / block_size) * block_size, block_size));

    uint8_t *pattern_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(pattern_buf);
    spi_nand_flash_fill_buffer(pattern_buf, page_size / sizeof(uint32_t));
    TEST_ESP_OK(bdl->ops->write(bdl, pattern_buf, test_page * (uint64_t)page_size, page_size));
    free(pattern_buf);

    esp_blockdev_cmd_arg_ecc_status_t page_ecc_status = {
        .page_num = test_page,
        .ecc_status = 0xFF,
    };
    TEST_ESP_OK(bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_PAGE_ECC_STATUS, &page_ecc_status));

    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL read invalid args", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    uint32_t page_size = bdl->geometry.read_size;
    uint8_t *buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(buf);

    esp_err_t ret = bdl->ops->read(bdl, NULL, page_size, 0, page_size);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = bdl->ops->read(bdl, buf, page_size - 1, 0, page_size);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    uint64_t oob_addr = bdl->geometry.disk_size;
    if (oob_addr >= page_size) {
        ret = bdl->ops->read(bdl, buf, page_size, oob_addr - page_size / 2, page_size);
        TEST_ASSERT_TRUE(ret != ESP_OK);
    }

    /* Read that crosses page boundary returns error */
    size_t bad_len = page_size + 1;
    ret = bdl->ops->read(bdl, buf, bad_len, 0, bad_len);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    free(buf);
    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL write invalid args", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    uint32_t page_size = bdl->geometry.write_size;
    uint8_t *buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(buf);
    memset(buf, 0x55, page_size);

    esp_err_t ret = bdl->ops->write(bdl, NULL, 0, page_size);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = bdl->ops->write(bdl, buf, 1, page_size);
    TEST_ASSERT_TRUE(ret == ESP_ERR_INVALID_ARG || ret == ESP_ERR_INVALID_SIZE);

    ret = bdl->ops->write(bdl, buf, 0, page_size - 1);
    TEST_ASSERT_TRUE(ret == ESP_ERR_INVALID_ARG || ret == ESP_ERR_INVALID_SIZE);

    uint64_t oob = bdl->geometry.disk_size;
    if (oob >= page_size) {
        ret = bdl->ops->write(bdl, buf, oob - page_size + 1, page_size);
        TEST_ASSERT_TRUE(ret != ESP_OK);
    }

    free(buf);
    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL multi-page read and write", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    uint32_t page_size = bdl->geometry.write_size;
    uint32_t block_size = bdl->geometry.erase_size;
    const uint32_t num_pages = 3;
    uint32_t start_page = (uint32_t)(bdl->geometry.disk_size / page_size) / 2;
    if (start_page + num_pages > bdl->geometry.disk_size / page_size) {
        start_page = 0;
    }

    uint32_t start_block = start_page / (block_size / page_size);
    TEST_ESP_OK(bdl->ops->erase(bdl, (uint64_t)start_block * block_size, block_size));

    size_t total_len = num_pages * page_size;
    uint8_t *pattern_buf = (uint8_t *)heap_caps_malloc(total_len, MALLOC_CAP_DEFAULT);
    uint8_t *read_buf = (uint8_t *)heap_caps_malloc(total_len, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(pattern_buf);
    TEST_ASSERT_NOT_NULL(read_buf);

    spi_nand_flash_fill_buffer(pattern_buf, total_len / sizeof(uint32_t));
    TEST_ESP_OK(bdl->ops->write(bdl, pattern_buf, start_page * (uint64_t)page_size, total_len));
    memset(read_buf, 0, total_len);
    TEST_ESP_OK(bdl->ops->read(bdl, read_buf, total_len, start_page * (uint64_t)page_size, total_len));
    TEST_ASSERT_EQUAL(0, spi_nand_flash_check_buffer(read_buf, total_len / sizeof(uint32_t)));

    /* Read back in single-page chunks */
    uint8_t *chunk_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(chunk_buf);
    for (uint32_t i = 0; i < num_pages; i++) {
        memset(chunk_buf, 0, page_size);
        TEST_ESP_OK(bdl->ops->read(bdl, chunk_buf, page_size, start_page * (uint64_t)page_size + i * (uint64_t)page_size, page_size));
        TEST_ASSERT_EQUAL_MEMORY(pattern_buf + i * page_size, chunk_buf, page_size);
    }
    free(chunk_buf);

    free(pattern_buf);
    free(read_buf);
    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL release then create again (no use-after-free)", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);
    bdl->ops->release(bdl);

    bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    uint32_t page_size = bdl->geometry.write_size;
    uint8_t *buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(buf);
    memset(buf, 0xAA, page_size);
    TEST_ESP_OK(bdl->ops->write(bdl, buf, 0, page_size));
    memset(buf, 0, page_size);
    TEST_ESP_OK(bdl->ops->read(bdl, buf, page_size, 0, page_size));
    free(buf);

    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL sync returns success", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    esp_err_t ret = bdl->ops->sync(bdl);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL ioctl with NULL args returns error", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    esp_err_t ret = bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_NAND_FLASH_INFO, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_BAD_BLOCKS_COUNT, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL zero-length read and write", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    uint8_t buf = 0;
    esp_err_t ret = bdl->ops->read(bdl, &buf, 1, 0, 0);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = bdl->ops->write(bdl, &buf, 0, 0);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL sub-page (partial page) read", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    uint32_t page_size = bdl->geometry.write_size;
    uint32_t block_size = bdl->geometry.erase_size;
    size_t partial_len = (page_size >= 256) ? 256 : (page_size / 2);
    if (partial_len == 0) {
        partial_len = 1;
    }

    uint32_t test_page = (uint32_t)(bdl->geometry.disk_size / page_size) / 2;
    uint64_t addr = test_page * (uint64_t)page_size;
    TEST_ESP_OK(bdl->ops->erase(bdl, (addr / block_size) * block_size, block_size));

    uint8_t *pattern_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    uint8_t *read_buf = (uint8_t *)heap_caps_malloc(partial_len, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(pattern_buf);
    TEST_ASSERT_NOT_NULL(read_buf);

    spi_nand_flash_fill_buffer(pattern_buf, page_size / sizeof(uint32_t));
    TEST_ESP_OK(bdl->ops->write(bdl, pattern_buf, addr, page_size));

    memset(read_buf, 0, partial_len);
    TEST_ESP_OK(bdl->ops->read(bdl, read_buf, partial_len, addr, partial_len));
    TEST_ASSERT_EQUAL_MEMORY(pattern_buf, read_buf, partial_len);

    size_t offset = (page_size >= 128) ? 128 : 1;
    size_t len = (page_size > offset + 64) ? 64 : (page_size - offset);
    if (len > 0) {
        memset(read_buf, 0, len);
        TEST_ESP_OK(bdl->ops->read(bdl, read_buf, len, addr + offset, len));
        TEST_ASSERT_EQUAL_MEMORY((uint8_t *)pattern_buf + offset, read_buf, len);
    }

    free(pattern_buf);
    free(read_buf);
    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL multi-page spanning block boundary", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    uint32_t page_size = bdl->geometry.write_size;
    uint32_t block_size = bdl->geometry.erase_size;
    uint32_t pages_per_block = block_size / page_size;
    uint32_t num_pages = (uint32_t)(bdl->geometry.disk_size / page_size);
    if (pages_per_block < 2 || num_pages < pages_per_block + 2) {
        deinit_nand_flash(spi, bdl);
        return;
    }

    uint32_t last_page_of_block0 = pages_per_block - 1;
    uint32_t page_count = 2;
    uint64_t start_addr = last_page_of_block0 * (uint64_t)page_size;

    TEST_ESP_OK(bdl->ops->erase(bdl, 0, block_size));
    TEST_ESP_OK(bdl->ops->erase(bdl, block_size, block_size));

    size_t total_len = page_count * page_size;
    uint8_t *pattern_buf = (uint8_t *)heap_caps_malloc(total_len, MALLOC_CAP_DEFAULT);
    uint8_t *read_buf = (uint8_t *)heap_caps_malloc(total_len, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(pattern_buf);
    TEST_ASSERT_NOT_NULL(read_buf);

    spi_nand_flash_fill_buffer(pattern_buf, total_len / sizeof(uint32_t));
    TEST_ESP_OK(bdl->ops->write(bdl, pattern_buf, start_addr, total_len));
    memset(read_buf, 0, total_len);
    TEST_ESP_OK(bdl->ops->read(bdl, read_buf, total_len, start_addr, total_len));
    TEST_ASSERT_EQUAL_MEMORY(pattern_buf, read_buf, total_len);

    free(pattern_buf);
    free(read_buf);
    deinit_nand_flash(spi, bdl);
}

TEST_CASE("Flash BDL read at end of device (last byte)", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&nand_flash_config, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);

    uint64_t disk_size = bdl->geometry.disk_size;
    uint32_t block_size = (uint32_t)bdl->geometry.erase_size;
    uint64_t last_block_start = ((disk_size / block_size) - 1) * block_size;
    TEST_ESP_OK(bdl->ops->erase(bdl, last_block_start, block_size));

    uint8_t buf[1];
    esp_err_t ret = bdl->ops->read(bdl, buf, sizeof(buf), disk_size - 1, 1);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = bdl->ops->read(bdl, buf, sizeof(buf), disk_size, 1);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, ret);

    deinit_nand_flash(spi, bdl);
}

/* --- WL BDL tests (grouped together) --- */

TEST_CASE("WL BDL sync after write", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t wl_bdl;
    setup_nand_flash(&spi, SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX, &wl_bdl);

    uint32_t page_size = wl_bdl->geometry.write_size;
    uint8_t *pattern_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    uint8_t *read_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(pattern_buf);
    TEST_ASSERT_NOT_NULL(read_buf);

    spi_nand_flash_fill_buffer(pattern_buf, page_size / sizeof(uint32_t));
    TEST_ESP_OK(wl_bdl->ops->write(wl_bdl, pattern_buf, 0, page_size));
    TEST_ESP_OK(wl_bdl->ops->sync(wl_bdl));
    memset(read_buf, 0, page_size);
    TEST_ESP_OK(wl_bdl->ops->read(wl_bdl, read_buf, page_size, 0, page_size));
    TEST_ASSERT_EQUAL(0, spi_nand_flash_check_buffer(read_buf, page_size / sizeof(uint32_t)));

    free(pattern_buf);
    free(read_buf);
    deinit_nand_flash(spi, wl_bdl);
}

TEST_CASE("WL BDL ESP_BLOCKDEV_CMD_MARK_DELETED (TRIM)", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t wl_bdl;
    setup_nand_flash(&spi, SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX, &wl_bdl);

    uint32_t page_size = wl_bdl->geometry.write_size;
    uint32_t num_pages;
    wl_bdl->ops->ioctl(wl_bdl, ESP_BLOCKDEV_CMD_GET_AVAILABLE_SECTORS, &num_pages);
    TEST_ASSERT_TRUE(num_pages >= 4);

    uint8_t *pattern_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(pattern_buf);
    spi_nand_flash_fill_buffer(pattern_buf, page_size / sizeof(uint32_t));

    const uint32_t start_page = 1;
    const uint32_t page_count = 3;
    for (uint32_t i = start_page; i < start_page + page_count; i++) {
        TEST_ESP_OK(wl_bdl->ops->write(wl_bdl, pattern_buf, (uint64_t)i * page_size, page_size));
    }

    esp_blockdev_cmd_arg_erase_t trim_arg = {
        .start_addr = (uint64_t)start_page * page_size,
        .erase_len = (size_t)page_count * page_size,
    };
    TEST_ESP_OK(wl_bdl->ops->ioctl(wl_bdl, ESP_BLOCKDEV_CMD_MARK_DELETED, &trim_arg));

    free(pattern_buf);
    deinit_nand_flash(spi, wl_bdl);
}

TEST_CASE("WL BDL read invalid args", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t wl_bdl;
    setup_nand_flash(&spi, SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX, &wl_bdl);

    uint32_t page_size = wl_bdl->geometry.read_size;
    uint8_t *buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(buf);

    esp_err_t ret = wl_bdl->ops->read(wl_bdl, NULL, page_size, 0, page_size);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = wl_bdl->ops->read(wl_bdl, buf, page_size - 1, 0, page_size);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = wl_bdl->ops->read(wl_bdl, buf, page_size, wl_bdl->geometry.disk_size, page_size);
    TEST_ASSERT_TRUE(ret != ESP_OK);

    free(buf);
    deinit_nand_flash(spi, wl_bdl);
}

TEST_CASE("WL BDL write invalid args", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t wl_bdl;
    setup_nand_flash(&spi, SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX, &wl_bdl);

    uint32_t page_size = wl_bdl->geometry.write_size;
    uint8_t *buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(buf);
    memset(buf, 0x55, page_size);

    esp_err_t ret = wl_bdl->ops->write(wl_bdl, NULL, 0, page_size);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = wl_bdl->ops->write(wl_bdl, buf, 1, page_size);
    TEST_ASSERT_TRUE(ret == ESP_ERR_INVALID_ARG || ret == ESP_ERR_INVALID_SIZE);

    ret = wl_bdl->ops->write(wl_bdl, buf, wl_bdl->geometry.disk_size, page_size);
    TEST_ASSERT_TRUE(ret != ESP_OK);

    free(buf);
    deinit_nand_flash(spi, wl_bdl);
}

TEST_CASE("WL BDL erase invalid args", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t wl_bdl;
    setup_nand_flash(&spi, SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX, &wl_bdl);

    uint32_t erase_size = (uint32_t)wl_bdl->geometry.erase_size;

    esp_err_t ret = wl_bdl->ops->erase(wl_bdl, erase_size / 2, erase_size);
    TEST_ASSERT_TRUE(ret == ESP_ERR_INVALID_ARG || ret == ESP_ERR_INVALID_SIZE);

    ret = wl_bdl->ops->erase(wl_bdl, wl_bdl->geometry.disk_size, erase_size);
    TEST_ASSERT_TRUE(ret != ESP_OK);

    deinit_nand_flash(spi, wl_bdl);
}

TEST_CASE("WL BDL zero-length read and write", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t wl_bdl;
    setup_nand_flash(&spi, SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX, &wl_bdl);

    uint8_t buf = 0;
    esp_err_t ret = wl_bdl->ops->read(wl_bdl, &buf, 1, 0, 0);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = wl_bdl->ops->write(wl_bdl, &buf, 0, 0);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    deinit_nand_flash(spi, wl_bdl);
}

TEST_CASE("WL BDL GET_AVAILABLE_SECTORS after full erase", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t wl_bdl;
    setup_nand_flash(&spi, SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX, &wl_bdl);

    uint32_t num_before = 0;
    TEST_ESP_OK(wl_bdl->ops->ioctl(wl_bdl, ESP_BLOCKDEV_CMD_GET_AVAILABLE_SECTORS, &num_before));

    /* Erase length must be aligned to erase_size */
    size_t erase_size = (size_t)wl_bdl->geometry.erase_size;
    size_t erase_len = (size_t)((wl_bdl->geometry.disk_size / erase_size) * erase_size);
    TEST_ESP_OK(wl_bdl->ops->erase(wl_bdl, 0, erase_len));

    uint32_t num_after = 0;
    TEST_ESP_OK(wl_bdl->ops->ioctl(wl_bdl, ESP_BLOCKDEV_CMD_GET_AVAILABLE_SECTORS, &num_after));

    uint32_t expected_pages = (uint32_t)(wl_bdl->geometry.disk_size / wl_bdl->geometry.write_size);
    TEST_ASSERT_EQUAL_UINT32(expected_pages, num_after);

    deinit_nand_flash(spi, wl_bdl);
}

TEST_CASE("WL BDL write N pages read back in different chunk sizes", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t wl_bdl;
    setup_nand_flash(&spi, SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX, &wl_bdl);

    uint32_t page_size = wl_bdl->geometry.write_size;
    uint32_t num_pages = 0;
    TEST_ESP_OK(wl_bdl->ops->ioctl(wl_bdl, ESP_BLOCKDEV_CMD_GET_AVAILABLE_SECTORS, &num_pages));
    TEST_ASSERT_TRUE(num_pages >= 3);

    const uint32_t n = 3;
    size_t total_len = n * page_size;
    uint8_t *pattern_buf = (uint8_t *)heap_caps_malloc(total_len, MALLOC_CAP_DEFAULT);
    uint8_t *read_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(pattern_buf);
    TEST_ASSERT_NOT_NULL(read_buf);

    spi_nand_flash_fill_buffer(pattern_buf, total_len / sizeof(uint32_t));
    TEST_ESP_OK(wl_bdl->ops->write(wl_bdl, pattern_buf, 0, total_len));

    for (uint32_t i = 0; i < n; i++) {
        memset(read_buf, 0, page_size);
        TEST_ESP_OK(wl_bdl->ops->read(wl_bdl, read_buf, page_size, i * (uint64_t)page_size, page_size));
        TEST_ASSERT_EQUAL_MEMORY(pattern_buf + i * page_size, read_buf, page_size);
    }

    free(pattern_buf);
    free(read_buf);
    deinit_nand_flash(spi, wl_bdl);
}

TEST_CASE("WL BDL unaligned read and write length returns error", "[spi_nand_flash][bdl]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t wl_bdl;
    setup_nand_flash(&spi, SPI_NAND_IO_MODE_SIO, SPI_DEVICE_HALFDUPLEX, &wl_bdl);

    uint32_t page_size = wl_bdl->geometry.read_size;
    uint8_t *buf = (uint8_t *)heap_caps_malloc(page_size + 1, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(buf);

    esp_err_t ret = wl_bdl->ops->read(wl_bdl, buf, page_size + 1, 0, page_size + 1);
    TEST_ASSERT_TRUE(ret == ESP_ERR_INVALID_ARG || ret == ESP_ERR_INVALID_SIZE);

    memset(buf, 0x55, page_size + 1);
    ret = wl_bdl->ops->write(wl_bdl, buf, 0, page_size + 1);
    TEST_ASSERT_TRUE(ret == ESP_ERR_INVALID_ARG || ret == ESP_ERR_INVALID_SIZE);

    free(buf);
    deinit_nand_flash(spi, wl_bdl);
}
