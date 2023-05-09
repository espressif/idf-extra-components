/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "onewire_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 1-Wire device generic type
 */
typedef struct onewire_device_t {
    onewire_bus_handle_t bus;         /*!< Which bus the 1-Wire device is attached to */
    onewire_device_address_t address; /*!< Device address (represented by its internal ROM ID) */
} onewire_device_t;

/**
 * @brief Create an iterator to enumerate the 1-Wire devices on the bus
 *
 * @param[in] bus 1-Wire bus handle
 * @param[out] ret_iter Returned created device iterator
 * @return
 *      - ESP_OK: Create device iterator successfully
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - ESP_ERR_NO_MEM: No memory to create device iterator
 *      - ESP_FAIL: Other errors
 */
esp_err_t onewire_new_device_iter(onewire_bus_handle_t bus, onewire_device_iter_handle_t *ret_iter);

/**
 * @brief Delete the device iterator
 *
 * @param[in] iter Device iterator handle
 * @return
 *      - ESP_OK: Delete device iterator successfully
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - ESP_FAIL: Other errors
 */
esp_err_t onewire_del_device_iter(onewire_device_iter_handle_t iter);

/**
 * @brief Get the next 1-Wire device from the iterator
 *
 * @param[in] iter Device iterator handle
 * @param[out] dev Returned 1-Wire device handle
 * @return
 *      - ESP_OK: Get next device successfully
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - ESP_ERR_NOT_FOUND: No more device to get
 *      - ESP_FAIL: Other errors
 */
esp_err_t onewire_device_iter_get_next(onewire_device_iter_handle_t iter, onewire_device_t *dev);

#ifdef __cplusplus
}
#endif
