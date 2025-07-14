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
 * @brief Type of UART EMU device handle
 */
typedef struct uart_emu_device_t *uart_emu_device_handle_t;

typedef enum {
    UART_EMU_DATA_8_BITS   = 0x8,    /*!< word length: 8bits*/
} uart_emu_word_length_t;

typedef enum {
    UART_EMU_STOP_BITS_1   = 0x0,  /*!< stop bit: 1bit*/
    UART_EMU_STOP_BITS_2   = 0x1,  /*!< stop bit: 2bits*/
} uart_emu_stop_bits_t;

typedef enum {
    UART_EMU_PARITY_DISABLE = 0x0,  /*!< Disable UART parity*/
} uart_emu_parity_t;

/**
 * @brief UART EMU TX Done Event Data
 */
typedef struct {

} uart_emu_tx_done_event_data_t;

/**
 * @brief UART EMU TX Done Callback Function Type
 * @param uart_device Handle to the UART EMU device that initiated the transmission.
 * @param edata Pointer to a structure containing event data related to the completed transmission.
 * @param user_ctx User-defined context passed during the callback registration.
 *                 It can be used to maintain application-specific state or data.
 *
 * @return Whether a high priority task has been waken up by this callback function
 */
typedef bool (*uart_emu_tx_done_callback_t)(uart_emu_device_handle_t uart_device, const uart_emu_tx_done_event_data_t *edata, void *user_ctx);

/**
 * @brief UART EMU RX Done Event Data
 */
typedef struct {
    void *rx_done_event_data;
} uart_emu_rx_done_event_data_t;

/**
 * @brief UART EMU RX Done Callback Function Type
 * @param uart_device Handle to the UART EMU device that initiated the transmission.
 * @param edata Pointer to a structure containing event data related to the completed transmission.
 *              This structure provides details such as the number of bytes transmitted and any
 *              status information relevant to the operation.
 * @param user_ctx User-defined context passed during the callback registration.
 *                 It can be used to maintain application-specific state or data.
 *
 * @return Whether a high priority task has been waken up by this callback function
 */
typedef bool (*uart_emu_rx_done_callback_t)(uart_emu_device_handle_t uart_device, const uart_emu_rx_done_event_data_t *edata, void *user_ctx);

#ifdef __cplusplus
}
#endif
