/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2026 Espressif Systems (Shanghai) CO LTD
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

struct spi_nand_flash_device_t;

/**
 * @brief Attach default OOB layout and init-time field/cache metadata on the device handle.
 *
 * Call from nand_init_device after vendor @c detect_chip (ECC configuration applied) and spare
 * geometry are known. Not ISR-safe; task/init context only, same as nand_impl.
 *
 * @param chip_ctx for layout @c free_region is this handle (opaque @c const void * in ops).
 */
esp_err_t nand_oob_device_layout_init(struct spi_nand_flash_device_t *handle);

#ifdef __cplusplus
}
#endif
