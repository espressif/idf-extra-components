/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "tinyusb_types.h"
#include "esp_err.h"
#include "sdkconfig.h"

#if (CONFIG_TINYUSB_NET_MODE_NONE != 1)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ESP TinyUSB NCM driver configuration structure
 */
typedef struct {
    char *ssid;
    char *pwd;
} tinyusb_wifi_config_t;

/**
 * @brief Initialize TinyUSB NET-NCM driver
 *
 * @param[in] usb_dev USB device to use
 * @return esp_err_t
 */
esp_err_t tinyusb_net_init(tinyusb_usbdev_t usb_dev);

/**
 * @brief TinyUSB NET-NCM driver connect WiFi
 *
 * @param[in] cfg     Configuration of WiFi
 * @return esp_err_t
 */
esp_err_t tinyusb_net_connet_wifi(const tinyusb_wifi_config_t *cfg);

#endif

//TODO: Define ESP-specific functions
#ifdef __cplusplus
}
#endif
