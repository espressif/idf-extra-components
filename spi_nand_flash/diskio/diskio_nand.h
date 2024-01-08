/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2023 Espressif Systems (Shanghai) CO LTD
 */

#pragma once

#include "spi_nand_flash.h"
#include "ff.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register NAND flash diskio driver
 *
 * @param pdrv  drive number
 * @param device pointer to a nand flash device structure; device should be initialized before calling f_mount.
 */
esp_err_t ff_diskio_register_nand(BYTE pdrv, spi_nand_flash_device_t *device);

/**
 * @brief Get the driver number corresponding to a device
 *
 * @param device The device for which to return its driver
 * @return Driver number of the device
 */
BYTE ff_diskio_get_pdrv_nand(const spi_nand_flash_device_t *device);

/**
 * @brief Clear a registered nand driver, so it can be reused
 *
 * @param device The device for which to clear its registration
 */
void ff_diskio_clear_pdrv_nand(const spi_nand_flash_device_t *dev);

#ifdef __cplusplus
}
#endif
