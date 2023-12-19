/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2023 Espressif Systems (Shanghai) CO LTD
 */

#pragma once
#include <stddef.h>
#include "spi_nand_flash.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_SPI_FLASH_NAND_ENABLED
/**
 * @brief Convenience function to initialize FAT filesystem in SPI nand flash and register it in VFS
 *
 * This is an all-in-one function which does the following:
 *
 * - mounts FAT partition using FATFS library on top of nand flash
 * - registers FATFS library with VFS, with prefix given by base_prefix variable
 *
 * @param base_path        path where FATFS partition should be mounted (e.g. "/nandflash")
 * @param nand_device      nand device handle returned by spi_nand_flash_init_device
 * @param mount_config     pointer to structure with extra parameters for mounting FATFS
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NOT_FOUND if there are no more free fatfs slots
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_nand_mount was already called
 *      - ESP_ERR_NO_MEM if memory can not be allocated
 *      - other error codes from nand driver, SPI flash driver, or FATFS drivers
 */
esp_err_t esp_vfs_fat_nand_mount(const char* base_path,
                                 spi_nand_flash_device_t *nand_device,
                                 const esp_vfs_fat_mount_config_t* mount_config);

/**
 * @brief Unmount FAT filesystem and release resources acquired using esp_vfs_fat_nand_mount
 *
 * @param base_path  path where nand flash is mounted
 * @param nand_device  nand device handle used in mount
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_nand_mount hasn't been called
 */

esp_err_t esp_vfs_fat_nand_unmount(const char* base_path, spi_nand_flash_device_t *nand_device);
#endif // CONFIG_SPI_FLASH_NAND_ENABLED

#ifdef __cplusplus
}
#endif
