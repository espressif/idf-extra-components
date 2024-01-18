/*
 * SPDX-FileCopyrightText: 2020-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_private/usb_phy.h"
#include "soc/usb_pins.h"
#include "tinyusb.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configure and install new USB PHY
 *
 * @param[in] config tinyusb stack specific configuration
 */
esp_err_t tinyusb_ext_phy_new(const tinyusb_config_t *config);

/**
 * @brief Delete previously installed USB PHY
 *
 */
esp_err_t tinyusb_ext_phy_delete(void);

#ifdef __cplusplus
}
#endif
