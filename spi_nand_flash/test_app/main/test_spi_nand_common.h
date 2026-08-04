/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdint.h>
#include "driver/spi_master.h"
#include "sdkconfig.h"
#include "soc/spi_pins.h"

#ifdef CONFIG_IDF_TARGET_ESP32
#define HOST_ID      SPI3_HOST
#define PIN_MOSI     SPI3_IOMUX_PIN_NUM_MOSI
#define PIN_MISO     SPI3_IOMUX_PIN_NUM_MISO
#define PIN_CLK      SPI3_IOMUX_PIN_NUM_CLK
#define PIN_CS       SPI3_IOMUX_PIN_NUM_CS
#define PIN_WP       SPI3_IOMUX_PIN_NUM_WP
#define PIN_HD       SPI3_IOMUX_PIN_NUM_HD
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#else
#define HOST_ID      SPI2_HOST
#define PIN_MOSI     SPI2_IOMUX_PIN_NUM_MOSI
#define PIN_MISO     SPI2_IOMUX_PIN_NUM_MISO
#define PIN_CLK      SPI2_IOMUX_PIN_NUM_CLK
#define PIN_CS       SPI2_IOMUX_PIN_NUM_CS
#define PIN_WP       SPI2_IOMUX_PIN_NUM_WP
#define PIN_HD       SPI2_IOMUX_PIN_NUM_HD
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#endif

void spi_nand_test_setup_chip(spi_device_handle_t *spi, uint8_t flags);

/** For custom SPI init paths that do not call spi_nand_test_setup_chip(). */
void spi_nand_test_mark_bus_initialized(void);
void spi_nand_test_set_spi_handle(spi_device_handle_t spi);

/**
 * Free the SPI device and bus if still held.
 * Safe to call more than once. Used by test deinit and by Unity tearDown so
 * a failed TEST_ASSERT does not leave the bus initialized for the next case.
 */
void spi_nand_flash_test_teardown(void);
