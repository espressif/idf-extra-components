/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_blockdev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register block device diskio driver
 *
 * @param pdrv  drive number
 * @param blockdev pointer to a block device handle; blockdev should be initialized before calling f_mount.
 */
esp_err_t ff_diskio_register_blockdev(BYTE pdrv, esp_blockdev_handle_t blockdev);

/**
 * @brief Get the driver number corresponding to a block device
 *
 * @param blockdev The block device for which to return its driver
 * @return Driver number of the device, or 0xFF if not found
 */
BYTE ff_diskio_get_pdrv_blockdev(const esp_blockdev_handle_t blockdev);

/**
 * @brief Clear a registered block device driver, so it can be reused
 *
 * @param blockdev The block device for which to clear its registration
 */
void ff_diskio_clear_pdrv_blockdev(const esp_blockdev_handle_t blockdev);

#ifdef __cplusplus
}
#endif

