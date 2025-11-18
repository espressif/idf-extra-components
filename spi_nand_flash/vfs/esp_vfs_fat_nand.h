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

#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
#include "esp_blockdev.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

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
esp_err_t esp_vfs_fat_nand_mount(const char *base_path,
                                 spi_nand_flash_device_t *nand_device,
                                 const esp_vfs_fat_mount_config_t *mount_config);

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

esp_err_t esp_vfs_fat_nand_unmount(const char *base_path, spi_nand_flash_device_t *nand_device);

#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
/**
 * @brief Convenience function to initialize FAT filesystem using block device layer and register it in VFS
 *
 * This is an all-in-one function which does the following:
 *
 * - mounts FAT partition using FATFS library on top of a block device (e.g., wear-leveling BDL)
 * - registers FATFS library with VFS, with prefix given by base_prefix variable
 *
 * @note This API uses the block device layer (BDL) interface, which provides better abstraction
 *       and allows using wear-leveling or other block device layers directly.
 *
 * @param base_path        path where FATFS partition should be mounted (e.g. "/nandflash")
 * @param blockdev         block device handle (e.g., from spi_nand_flash_wl_get_blockdev)
 * @param mount_config     pointer to structure with extra parameters for mounting FATFS
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if blockdev is NULL or invalid
 *      - ESP_ERR_NOT_FOUND if there are no more free fatfs slots
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_nand_mount_bdl was already called
 *      - ESP_ERR_NO_MEM if memory can not be allocated
 *      - other error codes from block device driver, SPI flash driver, or FATFS drivers
 */
esp_err_t esp_vfs_fat_nand_mount_bdl(const char *base_path,
                                     esp_blockdev_handle_t blockdev,
                                     const esp_vfs_fat_mount_config_t *mount_config);

/**
 * @brief Unmount FAT filesystem and release resources acquired using esp_vfs_fat_nand_mount_bdl
 *
 * @param base_path  path where nand flash is mounted
 * @param blockdev   block device handle used in mount
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_nand_mount_bdl hasn't been called
 */
esp_err_t esp_vfs_fat_nand_unmount_bdl(const char *base_path, esp_blockdev_handle_t blockdev);
#endif // CONFIG_NAND_FLASH_ENABLE_BDL

#ifdef __cplusplus
}
#endif
