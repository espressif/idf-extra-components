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
 * Generic chip path: ONFI then Kconfig geometry and conservative profile.
 * Caller must have already read JEDEC/manufacturer ID into the device.
 * Does not call detect_chip() / vendor database.
 * Call nand_generic_detect_probe() after page_size and DMA buffers are ready.
 */
esp_err_t nand_generic_detect_init(spi_nand_flash_device_t *dev);

/** Non-destructive probes: GET FEATURE + page 0 read. Fills probe report. */
esp_err_t nand_generic_detect_probe(spi_nand_flash_device_t *dev);

#ifdef __cplusplus
}
#endif
