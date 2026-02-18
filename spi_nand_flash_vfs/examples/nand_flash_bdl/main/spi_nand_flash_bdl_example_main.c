/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "esp_system.h"
#include "soc/spi_pins.h"
#include "esp_vfs_fat_nand.h"
#include "esp_nand_blockdev.h"

#define EXAMPLE_FLASH_FREQ_KHZ      40000

static const char *TAG = "example_bdl";

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

// Mount path for the partition
const char *base_path = "/nand";

void app_main(void)
{
    esp_err_t ret;
    spi_device_handle_t spi = NULL;
    esp_blockdev_handle_t wl_bdl = NULL;

    // Initialize SPI bus
    const spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadhd_io_num = PIN_HD,
        .quadwp_io_num = PIN_WP,
        .max_transfer_sz = 4096 * 2,
    };

    ESP_LOGI(TAG, "DMA CHANNEL: %d", SPI_DMA_CHAN);
    ESP_ERROR_CHECK(spi_bus_initialize(HOST_ID, &bus_config, SPI_DMA_CHAN));

    const uint32_t spi_flags = SPI_DEVICE_HALFDUPLEX;
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = EXAMPLE_FLASH_FREQ_KHZ * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 10,
        .flags = spi_flags,
    };

    ESP_ERROR_CHECK(spi_bus_add_device(HOST_ID, &devcfg, &spi));

    // Create Flash Block Device Layer (this will initialize the device)
    spi_nand_flash_config_t config = {
        .device_handle = spi,
        .io_mode = SPI_NAND_IO_MODE_SIO,
        .flags = spi_flags,
        .gc_factor = 4,  // Wear leveling GC factor
    };

    ret = spi_nand_flash_init_with_layers(&config, &wl_bdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create BDL: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    // Mount FATFS using BDL
    esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .allocation_unit_size = 16 * 1024
    };

    ret = esp_vfs_fat_nand_mount_bdl(base_path, wl_bdl, &mount_config);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the flash memory to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        goto cleanup;
    }

    // Print FAT FS size information
    uint64_t bytes_total, bytes_free;
    esp_vfs_fat_info(base_path, &bytes_total, &bytes_free);
    ESP_LOGI(TAG, "FAT FS: %" PRIu64 " kB total, %" PRIu64 " kB free", bytes_total / 1024, bytes_free / 1024);

    // Create a file in FAT FS
    ESP_LOGI(TAG, "Opening file");
    FILE *f = fopen("/nand/hello_bdl.txt", "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        goto cleanup_fs;
    }
    fprintf(f, "Written using ESP-IDF %s with BDL API\n", esp_get_idf_version());
    fclose(f);
    ESP_LOGI(TAG, "File written");

    // Open file for reading
    ESP_LOGI(TAG, "Reading file");
    f = fopen("/nand/hello_bdl.txt", "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        goto cleanup_fs;
    }
    char line[128];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    esp_vfs_fat_info(base_path, &bytes_total, &bytes_free);
    ESP_LOGI(TAG, "FAT FS: %" PRIu64 " kB total, %" PRIu64 " kB free", bytes_total / 1024, bytes_free / 1024);

cleanup_fs:
    // Unmount FATFS
    esp_vfs_fat_nand_unmount_bdl(base_path, wl_bdl);

cleanup:
    // Release block device handles
    if (wl_bdl) {
        wl_bdl->ops->release(wl_bdl);
    }
    // Cleanup SPI bus
    if (spi) {
        ESP_ERROR_CHECK(spi_bus_remove_device(spi));
    }
    ESP_ERROR_CHECK(spi_bus_free(HOST_ID));
}

