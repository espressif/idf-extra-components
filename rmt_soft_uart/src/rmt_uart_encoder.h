/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "driver/rmt_encoder.h"
#include "rmt_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create RMT encoder for encoding uart frame into RMT symbols
 *
 * @param[in] config Encoder configuration
 * @param[out] ret_encoder Returned encoder handle
 * @return
 *      - ESP_ERR_INVALID_ARG for any invalid arguments
 *      - ESP_ERR_NO_MEM out of memory when creating uart encoder
 *      - ESP_OK if creating encoder successfully
 */
esp_err_t rmt_new_uart_encoder(const rmt_uart_config_t *uart_config, rmt_encoder_handle_t *ret_encoder);

#ifdef __cplusplus
}
#endif
