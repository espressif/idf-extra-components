/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "soc/soc_caps.h"
#include "hal/gpio_types.h"
#include "uart_emu_type.h"
#include "uart_emu.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t tx_trans_queue_depth;          /*!< RMT tx internal transfer queue depth, increase this value can support more transfers pending in the background */
    size_t tx_mem_block_symbols;          /*!< RMT tx mem block symbols*/
    size_t rx_mem_block_symbols;          /*!< RMT rx mem block symbols*/
    uint32_t intr_priority;               /*!< RMT interrupt priority*/
    struct {
        uint32_t with_dma: 1;             /*!< If set, the driver will allocate an RMT channel with DMA capability */
    } flags;                              /*!< RMT config flags */
} uart_emu_rmt_config_t;

/**
 * @brief Create a new UART EMU controller from RMT.
 *
 * @param[in] uart_config Configuration for the UART EMU controller.
 * @param[in] rmt_config Configuration for the RMT UART controller.
 * @param[out] ret_device Handle to the UART EMU controller.
 *
 * @return
 * - `ESP_OK`: UART EMU controller was successfully created.
 * - `ESP_ERR_INVALID_ARG`: Invalid arguments (e.g., null `uart_config` pointer or `ret_device` pointer).
 * - `ESP_ERR_NO_MEM`: No enough memory to create the UART EMU controller.
 */
esp_err_t uart_emu_new_from_rmt(const uart_emu_config_t *uart_config, const uart_emu_rmt_config_t *rmt_config, uart_emu_device_handle_t *ret_device);

/**
 * @brief Decode UART EMU RMT data.
 *
 * @note After receiving the UART EMU RMT symbols, you can use this function to decode the data.
 *
 * @param[in] uart_device Handle to the UART EMU controller, which was previously created by `uart_emu_new_from_rmt()`.
 * @param[in] rmt_rx_evt_data Pointer to the UART EMU RMT receive event data.
 * @param[in] rx_buf      Pointer to the buffer to store the decoded data.
 * @param[in] rx_buf_size Size of the buffer to store the decoded data.
 * @param[in] continue_on_error Whether to continue decoding on error.
 * @return
 *     - (0) Error
 *     - OTHERS (>0) The number of bytes decoded from UART EMU RMT received symbols
 */
int uart_emu_rmt_decode_data(uart_emu_device_handle_t uart_device, uart_emu_rx_done_event_data_t *rmt_rx_evt_data, uint8_t *rx_buf, size_t rx_buf_size, bool continue_on_error);

#ifdef __cplusplus
}
#endif
