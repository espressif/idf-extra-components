/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "esp_err.h"
#include "wear_levelling.h"
#if SOC_SDMMC_HOST_SUPPORTED
#include "driver/sdmmc_host.h"
#endif


#if SOC_SDMMC_HOST_SUPPORTED
/**
 * @brief Configuration structure for sdmmc initialization
 *
 * User configurable parameters that are used while
 * initializing the sdmmc media.
 */
typedef struct {
    sdmmc_card_t *card;
} tinyusb_msc_sdmmc_config_t;
#endif

/**
 * @brief Configuration structure for spiflash initialization
 *
 * User configurable parameters that are used while
 * initializing the SPI Flash media.
 */
typedef struct {
    wl_handle_t wl_handle;
} tinyusb_msc_spiflash_config_t;

/**
 * @brief Register storage type spiflash with tinyusb driver
 *
 * @param config pointer to the spiflash configuration
 * @return esp_err_t
 *       - ESP_OK, if success;
 *       - ESP_ERR_NO_MEM, if there was no memory to allocate storage components;
 */
esp_err_t tinyusb_msc_storage_init_spiflash(const tinyusb_msc_spiflash_config_t *config);

#if SOC_SDMMC_HOST_SUPPORTED
/**
 * @brief Register storage type sd-card with tinyusb driver
 *
 * @param config pointer to the sd card configuration
 * @return esp_err_t
 *       - ESP_OK, if success;
 *       - ESP_ERR_NO_MEM, if there was no memory to allocate storage components;
 */
esp_err_t tinyusb_msc_storage_init_sdmmc(const tinyusb_msc_sdmmc_config_t *config);
#endif
/**
 * @brief Deregister storage with tinyusb driver and frees the memory
 *
 */
void tinyusb_msc_storage_deinit(void);

/**
 * @brief Mount the storage partition locally on the firmware application.
 *
 * Get the available drive number. Register spi flash partition.
 * Connect POSIX and C standard library IO function with FATFS.
 * Mounts the partition.
 * This API is used by the firmware application. If the storage partition is
 * mounted by this API, host (PC) can't access the storage via MSC.
 *
 * @param base_path  path prefix where FATFS should be registered
 * @return esp_err_t
 *       - ESP_OK, if success;
 *       - ESP_ERR_NOT_FOUND if the maximum count of volumes is already mounted
 *       - ESP_ERR_NO_MEM if not enough memory or too many VFSes already registered;
 */
esp_err_t tinyusb_msc_storage_mount(const char *base_path);

/**
 * @brief Unmount the storage partition from the firmware application.
 *
 * Unmount the partition. Unregister diskio driver.
 * Unregister the SPI flash partition.
 * Finally, Un-register FATFS from VFS.
 * After this function is called, storage device can be seen (recognized) by host (PC).
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if FATFS is not registered in VFS
 */
esp_err_t tinyusb_msc_storage_unmount(void);

/**
 * @brief Get number of sectors in storage media
 *
 * @return usable size, in bytes
 */
uint32_t tinyusb_msc_storage_get_sector_count(void);

/**
 * @brief Get sector size of storage media
 *
 * @return sector count
 */
uint32_t tinyusb_msc_storage_get_sector_size(void);

/**
 * @brief Get status if storage media is exposed over USB to Host
 *
 * @return bool
 *      - true, if the storage media is exposed to Host
 *      - false, if the stoarge media is mounted on application (not exposed to Host)
 */
bool tinyusb_msc_storage_in_use_by_usb_host(void);

#ifdef __cplusplus
}
#endif
