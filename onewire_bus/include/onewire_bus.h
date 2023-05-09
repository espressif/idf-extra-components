/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "onewire_types.h"
#include "onewire_bus_impl_rmt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Write bytes to 1-wire bus
 *
 * @param[in] bus 1-Wire bus handle
 * @param[in] tx_data pointer to data to be sent
 * @param[in] tx_data_size size of data to be sent, in bytes
 * @return
 *      - ESP_OK: Write bytes to 1-Wire bus successfully
 *      - ESP_ERR_INVALID_ARG: Write bytes to 1-Wire bus failed because of invalid argument
 *      - ESP_FAIL: Write bytes to 1-Wire bus failed because of other errors
 */
esp_err_t onewire_bus_write_bytes(onewire_bus_handle_t bus, const uint8_t *tx_data, uint8_t tx_data_size);

/**
 * @brief Read bytes from 1-wire bus
 *
 * @param[in] bus 1-wire bus handle
 * @param[out] rx_buf pointer to buffer to store received data
 * @param[in] rx_buf_size size of buffer to store received data, in bytes
 * @return
 *      - ESP_OK: Read bytes from 1-Wire bus successfully
 *      - ESP_ERR_INVALID_ARG: Read bytes from 1-Wire bus failed because of invalid argument
 *      - ESP_FAIL: Read bytes from 1-Wire bus failed because of other errors
 */
esp_err_t onewire_bus_read_bytes(onewire_bus_handle_t bus, uint8_t *rx_buf, size_t rx_buf_size);

/**
 * @brief Write a bit to 1-wire bus, this is a blocking function
 *
 * @param[in] handle 1-wire bus handle
 * @param[in] tx_bit bit to transmit, 0 for zero bit, other for one bit
 * @return
 *         - ESP_OK                Write bit to 1-wire bus successfully.
 *         - ESP_ERR_INVALID_ARG   Invalid argument.
 */
esp_err_t onewire_bus_write_bit(onewire_bus_handle_t bus, uint8_t tx_bit);

/**
 * @brief Read a bit from 1-wire bus
 *
 * @param[in] handle 1-wire bus handle
 * @param[out] rx_bit received bit, 0 for zero bit, 1 for one bit
 * @return
 *         - ESP_OK                Read bit from 1-wire bus successfully.
 *         - ESP_ERR_INVALID_ARG   Invalid argument.
 */
esp_err_t onewire_bus_read_bit(onewire_bus_handle_t bus, uint8_t *rx_bit);

/**
 * @brief Send reset pulse to the bus, and check if there are devices attached to the bus
 *
 * @param[in] bus 1-Wire bus handle
 *
 * @return
 *      - ESP_OK: Reset 1-Wire bus successfully and find device on the bus
 *      - ESP_ERR_NOT_FOUND: Reset 1-Wire bus successfully but no device found on the bus
 *      - ESP_FAIL: Reset 1-Wire bus failed because of other errors
 */
esp_err_t onewire_bus_reset(onewire_bus_handle_t bus);

/**
 * @brief Free 1-Wire bus resources
 *
 * @param[in] bus 1-Wire bus handle
 *
 * @return
 *      - ESP_OK: Free resources successfully
 *      - ESP_FAIL: Free resources failed because error occurred
 */
esp_err_t onewire_bus_del(onewire_bus_handle_t bus);

#ifdef __cplusplus
}
#endif
