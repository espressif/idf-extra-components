/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
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
 * @brief 1-Wire bus UART specific configuration
 */
typedef struct {
    int uart_port_num; /*!< UART port number, e.g. UART_NUM_1 */
} onewire_bus_uart_config_t;

/**
 * @brief Create 1-Wire bus with UART backend
 *
 * @note TX and RX will both be configured to bus_config->bus_gpio_num.
 *       And this GPIO will be configured as open-drain mode.
 *
 * @param[in] bus_config 1-Wire bus configuration
 * @param[in] uart_config UART specific configuration
 * @param[out] ret_bus Returned 1-Wire bus handle
 * @return
 *      - ESP_OK: create 1-Wire bus handle successfully
 *      - ESP_ERR_INVALID_ARG: create 1-Wire bus handle failed because of invalid argument
 *      - ESP_ERR_NO_MEM: create 1-Wire bus handle failed because of out of memory
 *      - ESP_FAIL: create 1-Wire bus handle failed because some other error
 */
esp_err_t onewire_new_bus_uart(const onewire_bus_config_t *bus_config, const onewire_bus_uart_config_t *uart_config, onewire_bus_handle_t *ret_bus);

#ifdef __cplusplus
}
#endif
