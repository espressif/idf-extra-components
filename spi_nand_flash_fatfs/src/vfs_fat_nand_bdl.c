/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_blockdev.h"
#include "esp_vfs_fat.h"
#include "vfs_fat_internal.h"
#include "diskio_impl.h"
#include "diskio_bdl.h"

static const char *TAG = "vfs_fat_nand_bdl";

esp_err_t esp_vfs_fat_nand_bdl_format(esp_blockdev_handle_t bdl_handle,
                                      const esp_vfs_fat_mount_config_t *mount_config)
{
    ESP_RETURN_ON_FALSE(bdl_handle != NULL && bdl_handle != ESP_BLOCKDEV_HANDLE_INVALID,
                        ESP_ERR_INVALID_ARG, TAG, "invalid BDL handle");
    ESP_RETURN_ON_FALSE(mount_config != NULL, ESP_ERR_INVALID_ARG, TAG, "mount_config is NULL");

    BYTE pdrv = 0xFF;
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_ERROR(ff_diskio_get_drive(&pdrv), cleanup, TAG, "the maximum count of volumes is already mounted");

    ESP_GOTO_ON_ERROR(ff_diskio_register_bdl(pdrv, bdl_handle), cleanup, TAG,
                      "ff_diskio_register_bdl failed");

    char drv[3] = {(char)('0' + pdrv), ':', 0};

    WORD sec_size_w = 0;
    if (disk_ioctl(pdrv, GET_SECTOR_SIZE, &sec_size_w) != RES_OK) {
        ESP_LOGE(TAG, "failed to query sector size from diskio");
        ret = ESP_FAIL;
        goto cleanup;
    }

    const size_t sec_size = (size_t)sec_size_w;
    const size_t sec_num = (size_t)(bdl_handle->geometry.disk_size / sec_size);

    const size_t workbuf_size = 4096;
    void *workbuf = ff_memalloc(workbuf_size);
    if (workbuf == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    const size_t alloc_unit_size = esp_vfs_fat_get_allocation_unit_size(
                                       sec_size, mount_config->allocation_unit_size);
    ESP_LOGI(TAG, "Formatting BDL volume, allocation unit size=%d", (int)alloc_unit_size);

    const UINT root_dir_entries = (sec_size == 512) ? 16 : 128;
    const MKFS_PARM opt = {
        (BYTE)(FM_ANY | FM_SFD),
        (mount_config->use_one_fat ? 1 : 2),
        0,
        (sec_num <= 128 ? root_dir_entries : 0),
        alloc_unit_size,
    };

    const FRESULT fresult = f_mkfs(drv, &opt, workbuf, workbuf_size);
    free(workbuf);

    if (fresult != FR_OK) {
        ESP_LOGE(TAG, "f_mkfs failed (%d)", fresult);
        ret = ESP_FAIL;
    }

cleanup:
    ff_diskio_unregister(pdrv);
    ff_diskio_clear_pdrv_bdl(bdl_handle);
    return ret;
}
