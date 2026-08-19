/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/*
 * FatFS over BDL on SPI NAND (wear-leveling stack)
 *
 * Stack (bottom to top):
 *
 *     +----------------+
 *     |  FatFS + VFS   |   esp_vfs_fat_bdl_mount / esp_vfs_fat_bdl_unmount
 *     +----------------+
 *     |   diskio_bdl   |   (ESP-IDF fatfs)
 *     +----------------+
 *     |    WL BDL      |   spi_nand_flash_wl_get_blockdev (Dhara FTL)
 *     +----------------+
 *     |  Flash BDL     |   nand_flash_get_blockdev (raw NAND pages/blocks)
 *     +----------------+
 *     |  SPI + chip    |   spi_nand_flash driver
 *     +----------------+
 *
 * Compare with examples/nand_flash: that path uses esp_vfs_fat_nand_mount() and the
 * legacy spi_nand_flash_init_device() API (CONFIG_NAND_FLASH_ENABLE_BDL must be off).
 */

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_blockdev.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "esp_vfs_fat_nand.h"
#include "driver/spi_master.h"
#include "soc/spi_pins.h"
#include "spi_nand_flash.h"

static const char *TAG = "example";

#define EXAMPLE_FLASH_FREQ_KHZ 40000

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

static const char *base_path = "/nandflash";

static void example_spi_nand_setup(spi_device_handle_t *out_spi, spi_nand_flash_config_t *out_cfg)
{
    const spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadhd_io_num = PIN_HD,
        .quadwp_io_num = PIN_WP,
        .max_transfer_sz = 4096 * 2,
    };

    ESP_LOGI(TAG, "DMA channel: %d", SPI_DMA_CHAN);
    ESP_ERROR_CHECK(spi_bus_initialize(HOST_ID, &bus_config, SPI_DMA_CHAN));

    const uint32_t spi_flags = SPI_DEVICE_HALFDUPLEX;

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = EXAMPLE_FLASH_FREQ_KHZ * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 10,
        .flags = spi_flags,
    };

    spi_device_handle_t spi = NULL;
    ESP_ERROR_CHECK(spi_bus_add_device(HOST_ID, &devcfg, &spi));

    out_cfg->device_handle = spi;
    out_cfg->io_mode = SPI_NAND_IO_MODE_SIO;
    out_cfg->flags = spi_flags;
    out_cfg->gc_factor = 0; /* driver default */

    assert(devcfg.flags == out_cfg->flags);
    *out_spi = spi;
}

static void example_spi_nand_teardown(spi_device_handle_t spi)
{
    ESP_ERROR_CHECK(spi_bus_remove_device(spi));
    ESP_ERROR_CHECK(spi_bus_free(HOST_ID));
}

static void example_release_bdl_logged(esp_blockdev_handle_t wl_bdl)
{
    esp_err_t err = wl_bdl->ops->release(wl_bdl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wl_bdl->ops->release failed: %s", esp_err_to_name(err));
    }
}

static void example_log_fat_info(const char *path)
{
    uint64_t bytes_total = 0;
    uint64_t bytes_free = 0;
    esp_err_t err = esp_vfs_fat_info(path, &bytes_total, &bytes_free);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_vfs_fat_info failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "FAT FS: %" PRIu64 " kB total, %" PRIu64 " kB free",
             bytes_total / 1024, bytes_free / 1024);
}

void app_main(void)
{
    spi_device_handle_t spi = NULL;
    spi_nand_flash_config_t nand_cfg = {0};

    example_spi_nand_setup(&spi, &nand_cfg);

    esp_blockdev_handle_t wl_bdl = NULL;
    esp_err_t ret = spi_nand_flash_init_with_layers(&nand_cfg, &wl_bdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_nand_flash_init_with_layers failed: %s", esp_err_to_name(ret));
        example_spi_nand_teardown(spi);
        return;
    }

    ESP_LOGI(TAG, "WL BDL: disk_size=%llu bytes, logical sector write_size=%u",
             (unsigned long long)wl_bdl->geometry.disk_size,
             (unsigned)wl_bdl->geometry.write_size);

    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .allocation_unit_size = 16 * 1024,
    };

#ifdef CONFIG_EXAMPLE_FORMAT_BEFORE_MOUNT
    ret = esp_vfs_fat_nand_bdl_format(wl_bdl, &mount_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_vfs_fat_nand_bdl_format failed: %s", esp_err_to_name(ret));
        example_release_bdl_logged(wl_bdl);
        example_spi_nand_teardown(spi);
        return;
    }
    ESP_LOGI(TAG, "Pre-mount format finished");
#endif

    ret = esp_vfs_fat_bdl_mount(base_path, wl_bdl, &mount_config);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Mount failed. Enable CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED to format on failure.");
        } else {
            ESP_LOGE(TAG, "esp_vfs_fat_bdl_mount failed: %s", esp_err_to_name(ret));
        }
        example_release_bdl_logged(wl_bdl);
        example_spi_nand_teardown(spi);
        return;
    }

    example_log_fat_info(base_path);

    const char *path = "/nandflash/hello.txt";
    ESP_LOGI(TAG, "Opening file");
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        esp_vfs_fat_bdl_unmount(base_path, wl_bdl);
        example_release_bdl_logged(wl_bdl);
        example_spi_nand_teardown(spi);
        return;
    }
    fprintf(f, "Written using ESP-IDF %s (FatFS via BDL)\n", esp_get_idf_version());
    fclose(f);
    ESP_LOGI(TAG, "File written");

    ESP_LOGI(TAG, "Reading file");
    f = fopen(path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        esp_vfs_fat_bdl_unmount(base_path, wl_bdl);
        example_release_bdl_logged(wl_bdl);
        example_spi_nand_teardown(spi);
        return;
    }
    char line[128] = {0};
    if (fgets(line, sizeof(line), f) == NULL) {
        ESP_LOGE(TAG, "Failed to read file");
        fclose(f);
        esp_vfs_fat_bdl_unmount(base_path, wl_bdl);
        example_release_bdl_logged(wl_bdl);
        example_spi_nand_teardown(spi);
        return;
    }
    fclose(f);
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    example_log_fat_info(base_path);

    ESP_LOGI(TAG, "Unmounting FAT filesystem");
    ESP_ERROR_CHECK(esp_vfs_fat_bdl_unmount(base_path, wl_bdl));

    ESP_LOGI(TAG, "Releasing BDL stack (WL releases underlying flash BDL)");
    ESP_ERROR_CHECK(wl_bdl->ops->release(wl_bdl));

    example_spi_nand_teardown(spi);
    ESP_LOGI(TAG, "Done");
}
