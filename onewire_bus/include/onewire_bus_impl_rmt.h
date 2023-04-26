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
 * @brief 1-Wire bus RMT specific configuration
 */
typedef struct {
    uint32_t max_rx_bytes; /*!< Set the largest possible single receive size,
                                which determins the size of the internal buffer that used to save the receiving RMT symbols */
} onewire_bus_rmt_config_t;

/**
 * @brief Create 1-Wire bus with RMT backend
 *
 * @note One 1-Wire bus utilizes a pair of RMT TX and RX channels
 *
 * @param[in] bus_config 1-Wire bus configuration
 * @param[in] rmt_config RMT specific configuration
 * @param[out] ret_bus Returned 1-Wire bus handle
 * @return
 *      - ESP_OK: create 1-Wire bus handle successfully
 *      - ESP_ERR_INVALID_ARG: create 1-Wire bus handle failed because of invalid argument
 *      - ESP_ERR_NO_MEM: create 1-Wire bus handle failed because of out of memory
 *      - ESP_FAIL: create 1-Wire bus handle failed because some other error
 */
esp_err_t onewire_new_bus_rmt(const onewire_bus_config_t *bus_config, const onewire_bus_rmt_config_t *rmt_config, onewire_bus_handle_t *ret_bus);

#ifdef __cplusplus
}
#endif
