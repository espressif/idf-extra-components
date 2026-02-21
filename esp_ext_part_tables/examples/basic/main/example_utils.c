/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"

#if !CONFIG_IDF_TARGET_LINUX
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#if SOC_SDMMC_IO_POWER_EXTERNAL
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif // SOC_SDMMC_IO_POWER_EXTERNAL
#endif // !CONFIG_IDF_TARGET_LINUX

#include "esp_ext_part_tables.h"

static const char *TAG = "esp_ext_part_tables_example_basic_utils";

#define PIN_NUM_MISO  CONFIG_EXAMPLE_PIN_MISO
#define PIN_NUM_MOSI  CONFIG_EXAMPLE_PIN_MOSI
#define PIN_NUM_CLK   CONFIG_EXAMPLE_PIN_CLK
#define PIN_NUM_CS    CONFIG_EXAMPLE_PIN_CS

#if CONFIG_IDF_TARGET_LINUX
// MBR with 2 FAT12 entries
uint8_t mbr_bin[512] = {
    [440] = 0xc4, 0x9d, 0x92, 0x4d, 0x00, 0x00, 0x00, 0x20, 0x21, 0x00,
    0x01, 0x9e, 0x2f, 0x00, 0x00, 0x08, 0x00, 0x00, 0x11, 0x1f,
    0x00, 0x00, 0x00, 0xa2, 0x23, 0x00, 0x01, 0x46, 0x05, 0x01,
    0x00, 0x28, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x55, 0xaa
};

unsigned int mbr_bin_len = 512;
#endif // CONFIG_IDF_TARGET_LINUX

esp_err_t load_first_sector_from_sd_card(void *mbr_buffer)
{
    ESP_LOGI(TAG, "Loading first sector from SD card");
#if CONFIG_IDF_TARGET_LINUX
    memcpy(mbr_buffer, mbr_bin, mbr_bin_len);
#else
    // This function loads the first sector (MBR) from the SD card into the provided buffer
    // It uses SDSPI but can be adapted for SDMMC as well
    esp_err_t ret = ESP_OK;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // For SoCs where the SD power can be supplied both via an internal or external (e.g. on-board LDO) power supply.
    // When using specific IO pins (which can be used for ultra high-speed SDMMC) to connect to the SD card
    // and the internal LDO power supply, we need to initialize the power supply first.
#if CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_INTERNAL_IO
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = CONFIG_EXAMPLE_SD_PWR_CTRL_LDO_IO_ID,
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;
#endif

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ret = host.init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize host.");
        spi_bus_free(host.slot);
        return ret;
    }

    int slot = -1;
    ret = sdspi_host_init_device((const sdspi_device_config_t *)&slot_config, &slot);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI device.");
        spi_bus_free(host.slot);
        return ret;
    }
    host.slot = slot;

    sdmmc_card_t card = {0};
    ret = sdmmc_card_init(&host, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SD card.");
        spi_bus_free(host.slot);
        return ret;
    }

    ret = sdmmc_read_sectors(&card, mbr_buffer, 0, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read first sector from SD card.");
        spi_bus_free(host.slot);
        return ret;
    }
#endif // !CONFIG_IDF_TARGET_LINUX
    return ESP_OK;
}

char *parsed_type_to_str(uint8_t type)
{
    switch (type) {
    case ESP_EXT_PART_TYPE_NONE:
        return "none/empty";
    case ESP_EXT_PART_TYPE_FAT12:
        return "FAT12";
    case ESP_EXT_PART_TYPE_FAT16:
        return "FAR16";
    case ESP_EXT_PART_TYPE_FAT32:
        return "FAT32";
    case ESP_EXT_PART_TYPE_LITTLEFS:
        return "LittleFS";
    default:
        break;
    }
    return "unknown";
}
