/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Type of 1-Wire bus handle
 */
typedef struct onewire_bus_t *onewire_bus_handle_t;

/**
 * @brief Type of the address for a 1-Wire compatible device
 */
typedef uint64_t onewire_device_address_t;

/**
 * @brief Type of 1-Wire device iterator handle
 */
typedef struct onewire_device_iter_t *onewire_device_iter_handle_t;

/**
 * @brief 1-Wire bus configuration
 */
typedef struct {
    int bus_gpio_num; /*!< GPIO number that used by the 1-Wire bus */
} onewire_bus_config_t;

#ifdef __cplusplus
}
#endif
