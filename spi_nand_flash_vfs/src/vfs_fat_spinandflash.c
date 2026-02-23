/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2023 Espressif Systems (Shanghai) CO LTD
 */

#include <stdlib.h>
#include <string.h>
#include <esp_check.h>
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "spi_nand_flash.h"
#include "vfs_fat_internal.h"
#include "diskio_impl.h"
#include "diskio_nand.h"

#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
#include "diskio_nand_blockdev.h"
#include "esp_blockdev.h"
#endif

static const char *TAG = "vfs_fat_nand";

esp_err_t esp_vfs_fat_nand_mount(const char *base_path, spi_nand_flash_device_t *nand_device,
                                 const esp_vfs_fat_mount_config_t *mount_config)
{
    esp_err_t ret = ESP_OK;
    void *workbuf = NULL;
    FATFS *fs = NULL;
    uint32_t sector_size;
    const size_t workbuf_size = 4096;

    // connect driver to FATFS
    BYTE pdrv = 0xFF;
    ESP_GOTO_ON_ERROR(ff_diskio_get_drive(&pdrv), fail, TAG, "the maximum count of volumes is already mounted");
    ESP_LOGD(TAG, "using pdrv=%i", pdrv);
    char drv[3] = {(char)('0' + pdrv), ':', 0};

    ESP_GOTO_ON_ERROR(ff_diskio_register_nand(pdrv, nand_device),
                      fail, TAG, "ff_diskio_register_nand failed drv=%i", pdrv);

    ESP_GOTO_ON_ERROR(spi_nand_flash_get_sector_size(nand_device, &sector_size), fail, TAG, "");

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    esp_vfs_fat_conf_t conf = {
        .base_path = base_path,
        .fat_drive = drv,
        .max_files = mount_config->max_files,
    };
    ESP_GOTO_ON_ERROR(esp_vfs_fat_register_cfg(&conf, &fs), fail, TAG, "esp_vfs_fat_register failed");
#else
    ESP_GOTO_ON_ERROR(esp_vfs_fat_register(base_path, drv, mount_config->max_files, &fs),
                      fail, TAG, "esp_vfs_fat_register failed");
#endif

    // Try to mount partition
    FRESULT fresult = f_mount(fs, drv, 1);
    if (fresult != FR_OK) {
        ESP_LOGW(TAG, "f_mount failed (%d)", fresult);
        if (!((fresult == FR_NO_FILESYSTEM || fresult == FR_INT_ERR)
                && mount_config->format_if_mount_failed)) {
            ret = ESP_FAIL;
            goto fail;
        }

        workbuf = ff_memalloc(workbuf_size);
        if (workbuf == NULL) {
            ret = ESP_ERR_NO_MEM;
            goto fail;
        }
        size_t alloc_unit_size = esp_vfs_fat_get_allocation_unit_size(
                                     sector_size,
                                     mount_config->allocation_unit_size);
        ESP_LOGI(TAG, "Formatting FATFS partition, allocation unit size=%d", alloc_unit_size);
        const MKFS_PARM opt = {(BYTE)FM_ANY, 0, 0, 0, alloc_unit_size};
        fresult = f_mkfs(drv, &opt, workbuf, workbuf_size);
        if (fresult != FR_OK) {
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "f_mkfs failed (%d)", fresult);
            goto fail;
        }
        free(workbuf);
        workbuf = NULL;
        ESP_LOGI(TAG, "Mounting again");
        fresult = f_mount(fs, drv, 0);
        if (fresult != FR_OK) {
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "f_mount failed after formatting (%d)", fresult);
            goto fail;
        }
    }
    return ESP_OK;

fail:
    if (workbuf) {
        free(workbuf);
    }
    if (fs) {
        esp_vfs_fat_unregister_path(base_path);
    }
    ff_diskio_unregister(pdrv);
    return ret;
}

esp_err_t esp_vfs_fat_nand_unmount(const char *base_path, spi_nand_flash_device_t *nand_device)
{
    BYTE pdrv = ff_diskio_get_pdrv_nand(nand_device);
    if (pdrv == 0xff) {
        return ESP_ERR_INVALID_STATE;
    }

    char drv[3] = {(char)('0' + pdrv), ':', 0};
    f_mount(NULL, drv, 0);

    ff_diskio_unregister(pdrv);
    ff_diskio_clear_pdrv_nand(nand_device);

    esp_err_t err = esp_vfs_fat_unregister_path(base_path);
    return err;
}

#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
esp_err_t esp_vfs_fat_nand_mount_bdl(const char *base_path, esp_blockdev_handle_t blockdev,
                                     const esp_vfs_fat_mount_config_t *mount_config)
{
    esp_err_t ret = ESP_OK;
    void *workbuf = NULL;
    FATFS *fs = NULL;
    size_t sector_size;
    const size_t workbuf_size = 4096;

    if (blockdev == NULL || blockdev->ops == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // connect driver to FATFS
    BYTE pdrv = 0xFF;
    ESP_GOTO_ON_ERROR(ff_diskio_get_drive(&pdrv), fail, TAG, "the maximum count of volumes is already mounted");
    ESP_LOGD(TAG, "using pdrv=%i", pdrv);
    char drv[3] = {(char)('0' + pdrv), ':', 0};

    ESP_GOTO_ON_ERROR(ff_diskio_register_blockdev(pdrv, blockdev),
                      fail, TAG, "ff_diskio_register_blockdev failed drv=%i", pdrv);

    sector_size = blockdev->geometry.read_size;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    esp_vfs_fat_conf_t conf = {
        .base_path = base_path,
        .fat_drive = drv,
        .max_files = mount_config->max_files,
    };
    ESP_GOTO_ON_ERROR(esp_vfs_fat_register_cfg(&conf, &fs), fail, TAG, "esp_vfs_fat_register failed");
#else
    ESP_GOTO_ON_ERROR(esp_vfs_fat_register(base_path, drv, mount_config->max_files, &fs),
                      fail, TAG, "esp_vfs_fat_register failed");
#endif

    // Try to mount partition
    FRESULT fresult = f_mount(fs, drv, 1);
    if (fresult != FR_OK) {
        ESP_LOGW(TAG, "f_mount failed (%d)", fresult);
        if (!((fresult == FR_NO_FILESYSTEM || fresult == FR_INT_ERR)
                && mount_config->format_if_mount_failed)) {
            ret = ESP_FAIL;
            goto fail;
        }

        workbuf = ff_memalloc(workbuf_size);
        if (workbuf == NULL) {
            ret = ESP_ERR_NO_MEM;
            goto fail;
        }
        size_t alloc_unit_size = esp_vfs_fat_get_allocation_unit_size(
                                     sector_size,
                                     mount_config->allocation_unit_size);
        ESP_LOGI(TAG, "Formatting FATFS partition, allocation unit size=%d", alloc_unit_size);
        const MKFS_PARM opt = {(BYTE)FM_ANY, 0, 0, 0, alloc_unit_size};
        fresult = f_mkfs(drv, &opt, workbuf, workbuf_size);
        if (fresult != FR_OK) {
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "f_mkfs failed (%d)", fresult);
            goto fail;
        }
        free(workbuf);
        workbuf = NULL;
        ESP_LOGI(TAG, "Mounting again");
        fresult = f_mount(fs, drv, 0);
        if (fresult != FR_OK) {
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "f_mount failed after formatting (%d)", fresult);
            goto fail;
        }
    }
    return ESP_OK;

fail:
    if (workbuf) {
        free(workbuf);
    }
    if (fs) {
        esp_vfs_fat_unregister_path(base_path);
    }
    ff_diskio_unregister(pdrv);
    return ret;
}

esp_err_t esp_vfs_fat_nand_unmount_bdl(const char *base_path, esp_blockdev_handle_t blockdev)
{
    BYTE pdrv = ff_diskio_get_pdrv_blockdev(blockdev);
    if (pdrv == 0xff) {
        return ESP_ERR_INVALID_STATE;
    }

    char drv[3] = {(char)('0' + pdrv), ':', 0};
    f_mount(NULL, drv, 0);

    ff_diskio_unregister(pdrv);
    ff_diskio_clear_pdrv_blockdev(blockdev);

    esp_err_t err = esp_vfs_fat_unregister_path(base_path);
    return err;
}
#endif // CONFIG_NAND_FLASH_ENABLE_BDL
