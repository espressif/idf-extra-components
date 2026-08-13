/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdbool.h>
#include "test_spi_nand_common.h"
#include "unity.h"

static bool s_spi_bus_initialized;
static spi_device_handle_t s_spi_handle;

static void setup_bus(void)
{
    spi_bus_config_t spi_bus_cfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadhd_io_num = PIN_HD,
        .quadwp_io_num = PIN_WP,
        .max_transfer_sz = 64,
    };
    TEST_ESP_OK(spi_bus_initialize(HOST_ID, &spi_bus_cfg, SPI_DMA_CHAN));
    s_spi_bus_initialized = true;
}

void spi_nand_test_setup_chip(spi_device_handle_t *spi, uint8_t flags)
{
    setup_bus();

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 10,
        .flags = flags,
    };

    TEST_ESP_OK(spi_bus_add_device(HOST_ID, &devcfg, spi));
    s_spi_handle = *spi;
}

void spi_nand_test_mark_bus_initialized(void)
{
    s_spi_bus_initialized = true;
}

void spi_nand_test_set_spi_handle(spi_device_handle_t spi)
{
    s_spi_handle = spi;
}

void spi_nand_flash_test_teardown(void)
{
    if (s_spi_handle) {
        spi_bus_remove_device(s_spi_handle);
        s_spi_handle = NULL;
    }
    if (s_spi_bus_initialized) {
        spi_bus_free(HOST_ID);
        s_spi_bus_initialized = false;
    }
}
