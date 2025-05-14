/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include "soc/soc_caps.h"
#include "driver/rmt_types.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Type of RMT channel handle
 */
typedef struct rmt_uart_device_t *rmt_uart_device_handle_t;

typedef enum {
    RMT_UART_DATA_8_BITS   = 0x8,    /*!< word length: 8bits*/
} rmt_uart_word_length_t;

typedef enum {
    RMT_UART_STOP_BITS_1   = 0x1,  /*!< stop bit: 1bit*/
    RMT_UART_STOP_BITS_2   = 0x2,  /*!< stop bit: 2bits*/
} rmt_uart_stop_bits_t;

typedef enum {
    RMT_UART_PARITY_DISABLE = 0x0,  /*!< Disable UART parity*/
    RMT_UART_PARITY_EVEN    = 0x1,  /*!< Enable UART even parity*/
    RMT_UART_PARITY_ODD     = 0x2,  /*!< Enable UART odd parity*/
} rmt_uart_parity_t;

/**
 * @brief RMT UART TX Done Event Data
 */
typedef struct {

} rmt_uart_tx_done_event_data_t;

/**
 * @brief RMT UART TX Done Callback Function Type
 * @param uart_device Handle to the RMT UART device that initiated the transmission.
 * @param edata Pointer to a structure containing event data related to the completed transmission.
 * @param user_ctx User-defined context passed during the callback registration.
 *                 It can be used to maintain application-specific state or data.
 *
 * @return Whether a high priority task has been waken up by this callback function
 */
typedef bool (*rmt_uart_tx_done_callback_t)(rmt_uart_device_handle_t uart_device, const rmt_uart_tx_done_event_data_t *edata, void *user_ctx);

/**
 * @brief RMT UART RX Done Event Data Structure
 */
typedef rmt_rx_done_event_data_t rmt_uart_rx_done_event_data_t;

/**
 * @brief RMT UART RX Done Callback Function Type
 * @param uart_device Handle to the RMT UART device that initiated the transmission.
 * @param edata Pointer to a structure containing event data related to the completed transmission.
 *              This structure provides details such as the number of bytes transmitted and any
 *              status information relevant to the operation.
 * @param user_ctx User-defined context passed during the callback registration.
 *                 It can be used to maintain application-specific state or data.
 *
 * @return Whether a high priority task has been waken up by this callback function
 */
typedef bool (*rmt_uart_rx_done_callback_t)(rmt_uart_device_handle_t uart_device, const rmt_uart_rx_done_event_data_t *edata, void *user_ctx);

#ifdef __cplusplus
}
#endif
