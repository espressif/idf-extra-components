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

typedef void (*net_recv_handler_t)(void *buffer, uint16_t len);

/**
 * @brief ESP TinyUSB NCM driver configuration structure
 */
typedef struct {
    const uint8_t *mac_addr; /*!< MAC address. Must be 6 bytes long. */
    net_recv_handler_t recv_handle; /*!< TinyUSB receive data handle */
} tinyusb_net_config_t;

/**
 * @brief Initialize TinyUSB NET driver
 *
 * @param[in] usb_dev USB device to use
 * @param[in] cfg     Configuration of the driver
 * @return esp_err_t
 */
esp_err_t tinyusb_net_init(tinyusb_usbdev_t usb_dev, const tinyusb_net_config_t *cfg);

/**
 * @brief TinyUSB NET driver send data
 *
 * @param[in] buffer USB send data
 * @param[in] len     Send data len
 * @return esp_err_t
 */
esp_err_t tinyusb_net_send(void *buffer, uint16_t len);

#endif

//TODO: Define ESP-specific functions
#ifdef __cplusplus
}
#endif
