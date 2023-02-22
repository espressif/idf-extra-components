/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_modem_c_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a DCE handle using the supplied USB DTE
 *
 * @param[in] module Specific device for creating this DCE
 * @param[in] dte_config DTE - USB configuration
 * @param[in] dce_config DCE configuration
 * @param[in] netif Network interface handle for the data mode
 *
 * @return DCE pointer on success, NULL on failure
 */
esp_modem_dce_t *esp_modem_new_dev_usb(esp_modem_dce_device_t module, const esp_modem_dte_config_t *dte_config, const esp_modem_dce_config_t *dce_config, esp_netif_t *netif);

#ifdef __cplusplus
}
#endif
