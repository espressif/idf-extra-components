/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2023 Espressif Systems (Shanghai) CO LTD
 */

#include "diskio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "diskio_nand.h"
#include "spi_nand_flash.h"
#include "diskio_impl.h"

static const char *TAG = "diskio_nand";

static spi_nand_flash_device_t *ff_nand_handles[FF_VOLUMES] = {NULL};

DSTATUS ff_nand_initialize(BYTE pdrv)
{
    return 0;
}

DSTATUS ff_nand_status(BYTE pdrv)
{
    return 0;
}

DRESULT ff_nand_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    ESP_LOGV(TAG, "ff_nand_read - pdrv=%i, sector=%i, count=%i", (unsigned int) pdrv, (unsigned int) sector,
             (unsigned int) count);
    esp_err_t ret;
    uint32_t sector_size;
    spi_nand_flash_device_t *dev = ff_nand_handles[pdrv];
    assert(dev);

    ESP_GOTO_ON_ERROR(spi_nand_flash_get_sector_size(dev, &sector_size), fail, TAG, "");

    for (uint32_t i = 0; i < count; i++) {
        ESP_GOTO_ON_ERROR(spi_nand_flash_read_sector(dev, buff + i * sector_size, sector + i),
                          fail, TAG, "spi_nand_flash_read failed");
    }

    return RES_OK;

fail:
    ESP_LOGE(TAG, "ff_nand_read failed with error 0x%X", ret);
    return RES_ERROR;
}

DRESULT ff_nand_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    ESP_LOGV(TAG, "ff_nand_write - pdrv=%i, sector=%i, count=%i", (unsigned int) pdrv, (unsigned int) sector,
             (unsigned int) count);
    esp_err_t ret;
    uint32_t sector_size;
    spi_nand_flash_device_t *dev = ff_nand_handles[pdrv];
    assert(dev);

    ESP_GOTO_ON_ERROR(spi_nand_flash_get_sector_size(dev, &sector_size), fail, TAG, "");

    for (uint32_t i = 0; i < count; i++) {
        ESP_GOTO_ON_ERROR(spi_nand_flash_write_sector(dev, buff + i * sector_size, sector + i),
                          fail, TAG, "spi_nand_flash_write failed");
    }
    return RES_OK;

fail:
    ESP_LOGE(TAG, "ff_nand_write failed with error 0x%X", ret);
    return RES_ERROR;
}

#if FF_USE_TRIM
DRESULT ff_nand_trim(BYTE pdrv, DWORD start_sector, DWORD sector_count)
{
    esp_err_t ret;
    spi_nand_flash_device_t *dev = ff_nand_handles[pdrv];
    assert(dev);

    uint32_t num_sectors;
    ESP_GOTO_ON_ERROR(spi_nand_flash_get_capacity(dev, &num_sectors),
                      fail, TAG, "get_capacity failed");

    if ((start_sector > num_sectors) || ((start_sector + sector_count) > num_sectors)) {
        return RES_PARERR;
    }

    for (uint32_t i = 0; i < sector_count; i++) {
        ESP_GOTO_ON_ERROR(spi_nand_flash_trim(dev, start_sector + i),
                          fail, TAG, "spi_nand_flash_trim failed");
    }
    return RES_OK;

fail:
    ESP_LOGE(TAG, "ff_nand_trim failed with error 0x%X", ret);
    return RES_ERROR;
}
#endif //FF_USE_TRIM

DRESULT ff_nand_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    spi_nand_flash_device_t *dev = ff_nand_handles[pdrv];
    assert(dev);

    ESP_LOGV(TAG, "ff_nand_ioctl: cmd=%i", cmd);
    esp_err_t ret = ESP_OK;
    switch (cmd) {
    case CTRL_SYNC:
        ESP_GOTO_ON_ERROR(spi_nand_flash_sync(dev), fail, TAG, "sync failed");
        break;
    case GET_SECTOR_COUNT: {
        uint32_t num_sectors;
        ESP_GOTO_ON_ERROR(spi_nand_flash_get_capacity(dev, &num_sectors),
                          fail, TAG, "get_capacity failed");
        *((DWORD *)buff) = num_sectors;
        ESP_LOGV(TAG, "capacity=%"PRIu32"", *((DWORD *) buff));
        break;
    }
    case GET_SECTOR_SIZE: {
        uint32_t sector_size;
        ESP_GOTO_ON_ERROR(spi_nand_flash_get_sector_size(dev, &sector_size),
                          fail, TAG, "get_sector_size failed");

        *((WORD *)buff) = sector_size;
        ESP_LOGV(TAG, "sector size=%d", *((WORD *)buff));
        break;
    }
#if FF_USE_TRIM
    case CTRL_TRIM: {
        DWORD start_sector = *((DWORD *)buff);
        DWORD end_sector = *((DWORD *)buff + 1) + 1;
        DWORD sector_count = end_sector - start_sector;
        return ff_nand_trim(pdrv, start_sector, sector_count);
    }
#endif //FF_USE_TRIM
    default:
        return RES_ERROR;
    }
    return RES_OK;

fail:
    ESP_LOGE(TAG, "ff_nand_ioctl cmd=%i, failed with error=0x%X", cmd, ret);
    return RES_ERROR;
}

esp_err_t ff_diskio_register_nand(BYTE pdrv, spi_nand_flash_device_t *device)
{
    if (pdrv >= FF_VOLUMES) {
        return ESP_ERR_INVALID_ARG;
    }

    static const ff_diskio_impl_t nand_impl = {
        .init = &ff_nand_initialize,
        .status = &ff_nand_status,
        .read = &ff_nand_read,
        .write = &ff_nand_write,
        .ioctl = &ff_nand_ioctl
    };
    ff_nand_handles[pdrv] = device;
    ff_diskio_register(pdrv, &nand_impl);
    return ESP_OK;
}

BYTE ff_diskio_get_pdrv_nand(const spi_nand_flash_device_t *dev)
{
    for (int i = 0; i < FF_VOLUMES; i++) {
        if (dev == ff_nand_handles[i]) {
            return i;
        }
    }
    return 0xff;
}

void ff_diskio_clear_pdrv_nand(const spi_nand_flash_device_t *dev)
{
    for (int i = 0; i < FF_VOLUMES; i++) {
        if (dev == ff_nand_handles[i]) {
            ff_nand_handles[i] = NULL;
        }
    }
}
