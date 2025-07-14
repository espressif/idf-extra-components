/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "driver/rmt_encoder.h"
#include "uart_emu_rmt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create RMT encoder for encoding uart frame into RMT symbols
 *
 * @param[in] uart_config Encoder configuration
 * @param[out] ret_encoder Returned encoder handle
 * @return
 *      - `ESP_OK`: Create uart encoder successfully
 *      - `ESP_ERR_INVALID_ARG`: Invalid arguments
 *      - `ESP_ERR_NO_MEM`: No enough memory to create uart encoder
 */
esp_err_t uart_emu_rmt_new_encoder(const uart_emu_config_t *uart_config, rmt_encoder_handle_t *ret_encoder);

#ifdef __cplusplus
}
#endif