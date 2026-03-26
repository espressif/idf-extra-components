/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "esp_system.h"
#include "soc/spi_pins.h"
#include "spi_nand_flash.h"
#include "spi_nand_flash_test_helpers.h"
#include "nand_diag_api.h"
#include "nand_private/nand_impl_wrap.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#define EXAMPLE_FLASH_FREQ_KHZ      40000

static const char *TAG = "debug_app";

// Pin mapping
// ESP32 (VSPI)
#ifdef CONFIG_IDF_TARGET_ESP32
#define HOST_ID  SPI3_HOST
#define PIN_MOSI SPI3_IOMUX_PIN_NUM_MOSI
#define PIN_MISO SPI3_IOMUX_PIN_NUM_MISO
#define PIN_CLK  SPI3_IOMUX_PIN_NUM_CLK
#define PIN_CS   SPI3_IOMUX_PIN_NUM_CS
#define PIN_WP   SPI3_IOMUX_PIN_NUM_WP
#define PIN_HD   SPI3_IOMUX_PIN_NUM_HD
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#else // Other chips (SPI2/HSPI)
#define HOST_ID  SPI2_HOST
#define PIN_MOSI SPI2_IOMUX_PIN_NUM_MOSI
#define PIN_MISO SPI2_IOMUX_PIN_NUM_MISO
#define PIN_CLK  SPI2_IOMUX_PIN_NUM_CLK
#define PIN_CS   SPI2_IOMUX_PIN_NUM_CS
#define PIN_WP   SPI2_IOMUX_PIN_NUM_WP
#define PIN_HD   SPI2_IOMUX_PIN_NUM_HD
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#endif

static void example_init_nand_flash(spi_nand_flash_device_t **out_handle, spi_device_handle_t *spi_handle)
{
    const spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadhd_io_num = PIN_HD,
        .quadwp_io_num = PIN_WP,
        .max_transfer_sz = 4096 * 2,
    };

    // Initialize the SPI bus
    ESP_LOGI(TAG, "DMA CHANNEL: %d", SPI_DMA_CHAN);
    ESP_ERROR_CHECK(spi_bus_initialize(HOST_ID, &bus_config, SPI_DMA_CHAN));

    // spi_flags = SPI_DEVICE_HALFDUPLEX -> half duplex
    // spi_flags = 0 -> full_duplex
    const uint32_t spi_flags = SPI_DEVICE_HALFDUPLEX;

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = EXAMPLE_FLASH_FREQ_KHZ * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 10,
        .flags = spi_flags,
    };

    spi_device_handle_t spi;
    ESP_ERROR_CHECK(spi_bus_add_device(HOST_ID, &devcfg, &spi));

    spi_nand_flash_config_t nand_flash_config = {
        .device_handle = spi,
        .io_mode = SPI_NAND_IO_MODE_SIO,
        .flags = spi_flags
    };
    spi_nand_flash_device_t *nand_flash_device_handle;
    ESP_ERROR_CHECK(spi_nand_flash_init_device(&nand_flash_config, &nand_flash_device_handle));

    *out_handle = nand_flash_device_handle;
    *spi_handle = spi;
}

static void example_deinit_nand_flash(spi_nand_flash_device_t *flash, spi_device_handle_t spi)
{
    ESP_ERROR_CHECK(spi_nand_flash_deinit_device(flash));
    ESP_ERROR_CHECK(spi_bus_remove_device(spi));
    ESP_ERROR_CHECK(spi_bus_free(HOST_ID));
}

static esp_err_t read_write_pages_tp(spi_nand_flash_device_t *flash, uint32_t start_page, uint16_t page_count, bool get_raw_tp)
{
    esp_err_t ret = ESP_OK;
    uint8_t *temp_buf = NULL;
    uint8_t *pattern_buf = NULL;
    uint32_t page_size, num_pages;

    ESP_ERROR_CHECK(spi_nand_flash_get_page_count(flash, &num_pages));
    ESP_ERROR_CHECK(spi_nand_flash_get_page_size(flash, &page_size));

    ESP_RETURN_ON_FALSE((start_page + page_count) < num_pages, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    pattern_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DMA);
    ESP_RETURN_ON_FALSE(pattern_buf != NULL, ESP_ERR_NO_MEM, TAG, "nomem");
    temp_buf = (uint8_t *)heap_caps_malloc(page_size, MALLOC_CAP_DMA);
    ESP_RETURN_ON_FALSE(temp_buf != NULL, ESP_ERR_NO_MEM, TAG, "nomem");

    spi_nand_flash_fill_buffer(pattern_buf, page_size / sizeof(uint32_t));

    int64_t read_time = 0;
    int64_t write_time = 0;

    for (uint32_t i = start_page; i < (start_page + page_count); i++) {
        int64_t start = esp_timer_get_time();
        if (get_raw_tp) {
            ESP_ERROR_CHECK(nand_wrap_prog(flash, i, pattern_buf));
        } else {
            ESP_ERROR_CHECK(spi_nand_flash_write_page(flash, pattern_buf, i));
        }
        write_time += esp_timer_get_time() - start;

        memset((void *)temp_buf, 0x00, page_size);

        start = esp_timer_get_time();
        if (get_raw_tp) {
            ESP_ERROR_CHECK(nand_wrap_read(flash, i, 0, page_size, temp_buf));
        } else {
            ESP_ERROR_CHECK(spi_nand_flash_read_page(flash, temp_buf, i));
        }
        read_time += esp_timer_get_time() - start;
    }
    free(pattern_buf);
    free(temp_buf);

    ESP_LOGI(TAG, "Wrote %" PRIu32 " bytes in %" PRId64 " us, avg %.2f kB/s", page_size * page_count, write_time, (float)page_size * page_count / write_time * 1000);
    ESP_LOGI(TAG, "Read %" PRIu32 " bytes in %" PRId64 " us, avg %.2f kB/s\n", page_size * page_count, read_time, (float)page_size * page_count / read_time * 1000);
    return ret;
}

void app_main(void)
{
    // Set up SPI bus and initialize the external SPI Flash chip
    spi_device_handle_t spi;
    spi_nand_flash_device_t *flash;
    example_init_nand_flash(&flash, &spi);
    if (flash == NULL) {
        return;
    }

    uint32_t num_blocks;
    ESP_ERROR_CHECK(spi_nand_flash_get_block_num(flash, &num_blocks));

    // Get bad block statistics
    uint32_t bad_block_count;
    ESP_LOGI(TAG, "Get bad block statistics:");
    ESP_ERROR_CHECK(nand_get_bad_block_stats(flash, &bad_block_count));
    ESP_LOGI(TAG, "\nTotal number of Blocks: %"PRIu32"\nBad Blocks: %"PRIu32"\nValid Blocks: %"PRIu32"\n",
             num_blocks, bad_block_count, num_blocks - bad_block_count);

    // Calculate read and write throughput via Dhara
    uint32_t start_page = 1;
    uint16_t page_count = 1000;
    bool get_raw_tp = false;
    ESP_LOGI(TAG, "Read-Write Throughput via Dhara:");
    ESP_ERROR_CHECK(read_write_pages_tp(flash, start_page, page_count, get_raw_tp));

    // Calculate read and write throughput at lower level (bypassing Dhara)
    start_page = 1001;
    page_count = 1000;
    get_raw_tp = true;
    ESP_LOGI(TAG, "Read-Write Throughput at lower level (bypassing Dhara):");
    ESP_ERROR_CHECK(read_write_pages_tp(flash, start_page, page_count, get_raw_tp));

    // Get ECC error statistics
    ESP_LOGI(TAG, "ECC errors statistics:");
    ESP_ERROR_CHECK(nand_get_ecc_stats(flash));

    example_deinit_nand_flash(flash, spi);
}
