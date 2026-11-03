/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "nand.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Apply Kconfig geometry/timing fallback for the generic detection path.
 * Returns ESP_ERR_NOT_FOUND if required geometry values are unset (0).
 * On success sets chip_source to SPI_NAND_CHIP_SOURCE_KCONFIG.
 */
esp_err_t nand_generic_geometry_try_init(spi_nand_flash_device_t *dev);

#ifdef __cplusplus
}
#endif
