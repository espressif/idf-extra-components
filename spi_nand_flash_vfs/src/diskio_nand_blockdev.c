/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include "diskio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "diskio_nand_blockdev.h"
#include "esp_blockdev.h"
#include "esp_nand_blockdev.h"
#include "diskio_impl.h"

static const char *TAG = "diskio_blockdev";

static esp_blockdev_handle_t ff_blockdev_handles[FF_VOLUMES] = {NULL};

DSTATUS ff_blockdev_initialize(BYTE pdrv)
{
    return 0;
}

DSTATUS ff_blockdev_status(BYTE pdrv)
{
    return 0;
}

DRESULT ff_blockdev_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    ESP_LOGV(TAG, "ff_blockdev_read - pdrv=%i, sector=%lu, count=%u", (unsigned int)pdrv, (unsigned long)sector, (unsigned int)count);
    esp_err_t ret;
    esp_blockdev_handle_t bdl = ff_blockdev_handles[pdrv];
    assert(bdl);

    size_t sector_size = bdl->geometry.read_size;

    for (uint32_t i = 0; i < count; i++) {
        ret = bdl->ops->read(bdl, buff + i * sector_size, sector_size, (sector + i) * sector_size, sector_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ff_blockdev_read failed with error 0x%X", ret);
            return RES_ERROR;
        }
    }

    return RES_OK;
}

DRESULT ff_blockdev_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    ESP_LOGV(TAG, "ff_blockdev_write - pdrv=%i, sector=%lu, count=%u", (unsigned int)pdrv, (unsigned long)sector, (unsigned int)count);
    esp_err_t ret;
    esp_blockdev_handle_t bdl = ff_blockdev_handles[pdrv];
    assert(bdl);

    size_t sector_size = bdl->geometry.write_size;

    for (uint32_t i = 0; i < count; i++) {
        ret = bdl->ops->write(bdl, buff + i * sector_size, (sector + i) * sector_size, sector_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ff_blockdev_write failed with error 0x%X", ret);
            return RES_ERROR;
        }
    }

    return RES_OK;
}

#if FF_USE_TRIM
DRESULT ff_blockdev_trim(BYTE pdrv, DWORD start_sector, DWORD sector_count)
{
    esp_err_t ret;
    esp_blockdev_handle_t bdl = ff_blockdev_handles[pdrv];
    assert(bdl);

    uint32_t num_sectors;
    ret = bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_AVAILABLE_SECTORS, &num_sectors);

    if ((start_sector > num_sectors) || ((start_sector + sector_count) > num_sectors)) {
        return RES_PARERR;
    }

    for (uint32_t sector = start_sector; sector < (start_sector + sector_count); sector++) {
        ret = bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_TRIM_SECTOR, &sector);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ff_blockdev_trim failed with error 0x%X", ret);
            return RES_ERROR;
        }
    }

    return RES_OK;
}
#endif //FF_USE_TRIM

DRESULT ff_blockdev_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    esp_blockdev_handle_t bdl = ff_blockdev_handles[pdrv];
    assert(bdl);

    ESP_LOGV(TAG, "ff_blockdev_ioctl: cmd=%i", cmd);
    esp_err_t ret = ESP_OK;

    switch (cmd) {
    case CTRL_SYNC:
        ret = bdl->ops->sync(bdl);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "sync failed with error 0x%X", ret);
            return RES_ERROR;
        }
        break;

    case GET_SECTOR_COUNT:
        uint32_t sector_count;
        ret = bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_AVAILABLE_SECTORS, &sector_count);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ioctl for GET_SECTOR_COUNT failed  with error 0x%X", ret);
            return RES_ERROR;
        }
        *((DWORD *)buff) = sector_count;
        ESP_LOGV(TAG, "capacity=%lu sectors", (unsigned long)(*((DWORD *)buff)));
        break;

    case GET_SECTOR_SIZE:
        *((WORD *)buff) = (WORD)bdl->geometry.read_size;
        ESP_LOGV(TAG, "sector size=%u", (unsigned int)(*((WORD *)buff)));
        break;

    case GET_BLOCK_SIZE: {
        uint32_t sec_per_block = bdl->geometry.erase_size / bdl->geometry.read_size;
        *((DWORD *)buff) = sec_per_block;
        ESP_LOGV(TAG, "block size=%lu sectors", (unsigned long)(*((DWORD *)buff)));
        break;
    }
#if FF_USE_TRIM
    case CTRL_TRIM: {
        DWORD start_sector = *((DWORD *)buff);
        DWORD end_sector = *((DWORD *)buff + 1) + 1;
        DWORD sector_count = end_sector - start_sector;
        return ff_blockdev_trim(pdrv, start_sector, sector_count);
    }
#endif //FF_USE_TRIM
    default:
        return RES_ERROR;
    }
    return RES_OK;
}

esp_err_t ff_diskio_register_blockdev(BYTE pdrv, esp_blockdev_handle_t blockdev)
{
    if (pdrv >= FF_VOLUMES) {
        return ESP_ERR_INVALID_ARG;
    }

    if (blockdev == NULL || blockdev->ops == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    static const ff_diskio_impl_t blockdev_impl = {
        .init = &ff_blockdev_initialize,
        .status = &ff_blockdev_status,
        .read = &ff_blockdev_read,
        .write = &ff_blockdev_write,
        .ioctl = &ff_blockdev_ioctl
    };
    ff_blockdev_handles[pdrv] = blockdev;
    ff_diskio_register(pdrv, &blockdev_impl);
    return ESP_OK;
}

BYTE ff_diskio_get_pdrv_blockdev(const esp_blockdev_handle_t blockdev)
{
    for (int i = 0; i < FF_VOLUMES; i++) {
        if (blockdev == ff_blockdev_handles[i]) {
            return i;
        }
    }
    return 0xff;
}

void ff_diskio_clear_pdrv_blockdev(const esp_blockdev_handle_t blockdev)
{
    for (int i = 0; i < FF_VOLUMES; i++) {
        if (blockdev == ff_blockdev_handles[i]) {
            ff_blockdev_handles[i] = NULL;
        }
    }
}

