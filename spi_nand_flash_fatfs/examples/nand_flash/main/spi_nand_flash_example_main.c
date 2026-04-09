/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "esp_system.h"
#include "soc/spi_pins.h"
#include "esp_vfs_fat_nand.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <sys/stat.h>

#define BOARD_NAND_FLASH_TEST

#ifndef BOARD_NAND_FLASH_TEST
#define EXAMPLE_FLASH_FREQ_KHZ      40000
#else
#define EXAMPLE_FLASH_FREQ_KHZ      40000
#endif
#define PATTERN_SEED    0x12345678

static const char *TAG = "example";

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
#ifndef BOARD_NAND_FLASH_TEST
#define PIN_MOSI SPI2_IOMUX_PIN_NUM_MOSI
#define PIN_MISO SPI2_IOMUX_PIN_NUM_MISO
#define PIN_CLK  SPI2_IOMUX_PIN_NUM_CLK
#define PIN_CS   SPI2_IOMUX_PIN_NUM_CS
#define PIN_WP   SPI2_IOMUX_PIN_NUM_WP
#define PIN_HD   SPI2_IOMUX_PIN_NUM_HD
#else
#define PIN_MOSI 11
#define PIN_MISO 13
#define PIN_CLK  12
#define PIN_CS   10
#define PIN_WP   14
#define PIN_HD   9
#endif
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#endif

// Mount path for the partition
const char *base_path = "/sdcard";

static void example_init_nand_flash(spi_nand_flash_device_t **out_handle, spi_device_handle_t *spi_handle)
{
#ifndef BOARD_NAND_FLASH_TEST
    const spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadhd_io_num = PIN_HD,
        .quadwp_io_num = PIN_WP,
        .max_transfer_sz = 4096 * 2,
    };
#else
    const spi_bus_config_t bus_config = {
        .data0_io_num = PIN_MOSI,
        .data1_io_num = PIN_MISO,
        .data2_io_num = PIN_WP,
        .data3_io_num = PIN_HD,
        .sclk_io_num = PIN_CLK,
        .max_transfer_sz = 4096 * 2,
    };
#endif

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
#ifndef BOARD_NAND_FLASH_TEST
        .io_mode = SPI_NAND_IO_MODE_SIO,
#else
        .io_mode = SPI_NAND_IO_MODE_QIO,
#endif
        .flags = spi_flags,
    };
    assert(devcfg.flags == nand_flash_config.flags);
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

void app_main(void)
{
    esp_err_t ret;
    // Set up SPI bus and initialize the external SPI Flash chip
    spi_device_handle_t spi;
    spi_nand_flash_device_t *flash;
    example_init_nand_flash(&flash, &spi);
    if (flash == NULL) {
        return;
    }

    esp_vfs_fat_mount_config_t config = {
        .max_files = 20,
        .format_if_mount_failed = true,
        .allocation_unit_size = 16 * 1024
    };

    // Erase chip before first use to ensure clean state
    ESP_LOGI(TAG, "Erasing NAND flash chip...");
    esp_err_t erase_ret = spi_nand_erase_chip(flash);
    if (erase_ret == ESP_ERR_NOT_FINISHED) {
        ESP_LOGW(TAG, "Some bad blocks encountered during erase, continuing...");
    } else if (erase_ret != ESP_OK) {
        ESP_ERROR_CHECK(erase_ret);
    }
    ESP_LOGI(TAG, "NAND flash chip erased");

    ret = esp_vfs_fat_nand_mount(base_path, flash, &config);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the flash memory to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        return;
    }

    // Print FAT FS size information
    uint64_t bytes_total, bytes_free;
    esp_vfs_fat_info(base_path, &bytes_total, &bytes_free);
    ESP_LOGI(TAG, "FAT FS: %" PRIu64 " kB total, %" PRIu64 " kB free", bytes_total / 1024, bytes_free / 1024);

    const char *data_dir = "/sdcard/data";

    struct stat st = {0};
    if (stat(data_dir, &st) == -1) {
        mkdir(data_dir, 0777);
    }
    ESP_LOGI(TAG, "Directory created");

    static char path0[64];
    static char line[128];
    char *pos;
    int tick;
    int round = 0;
    FILE *File;

    int i = 0;
    while (i < 10) {
        i++;
        tick = (int)xTaskGetTickCount();
        ESP_LOGI(TAG, "=== Round %d, tick %d ===", round, tick);

        snprintf(path0, 64, "%s/%d.txt", data_dir, tick);

        // Write file
        File = fopen(path0, "wb");
        if (!File) {
            ESP_LOGE(TAG, "fopen failed: errno=%d", errno);
        } else {
            fprintf(File, "tick=%d round=%d\n", tick, round);
            // fflush(File);
            // fsync(fileno(File));
            fclose(File);
            ESP_LOGI(TAG, "File written: %s", path0);
        }

        // Verify and read
        struct stat file_stat;
        if (stat(path0, &file_stat) == 0) {
            ESP_LOGI(TAG, "stat OK, size=%ld", file_stat.st_size);
            File = fopen(path0, "rb");
            if (File && fgets(line, 128, File)) {
                pos = strchr(line, '\n');
                if (pos) {
                    *pos = 0;
                }
                ESP_LOGI(TAG, "Read: %s", line);
                fclose(File);
            }
        } else {
            ESP_LOGE(TAG, "stat failed: errno=%d", errno);
        }

        esp_vfs_fat_info(base_path, &bytes_total, &bytes_free);
        ESP_LOGI(TAG, "FAT FS: %" PRIu64 " kB total, %" PRIu64 " kB free", bytes_total / 1024, bytes_free / 1024);

        round++;
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    example_deinit_nand_flash(flash, spi);
}
