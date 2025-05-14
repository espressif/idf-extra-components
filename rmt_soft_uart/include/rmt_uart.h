/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "soc/soc_caps.h"
#include "hal/gpio_types.h"
#include "rmt_uart_type.h"

typedef struct {
    uint32_t baud_rate;                   /*!< UART baud rate*/
    size_t tx_trans_queue_depth;          /*!< UART tx internal transfer queue depth, increase this value can support more transfers pending in the background */
    size_t rx_buffer_size;                  /*!< UART rx fifo size*/
    rmt_uart_word_length_t data_bits;     /*!< UART byte size*/
    rmt_uart_parity_t parity;             /*!< UART parity mode*/
    rmt_uart_stop_bits_t stop_bits;       /*!< UART stop bits*/
    gpio_num_t tx_io_num;                 /*!< UART tx io number*/
    gpio_num_t rx_io_num;                 /*!< UART rx io number*/
} rmt_uart_config_t;

/**
 * @brief Structure for defining callback functions for RMT UART transmission events.
 */
typedef struct {
    rmt_uart_tx_done_callback_t on_tx_trans_done;           /*!< Callback function for handling the completion of a transmission */
} rmt_uart_event_tx_callbacks_t;

/**
 * @brief Structure for defining callback functions for RMT UART receive events.
 */
typedef struct {
    rmt_uart_rx_done_callback_t on_rx_trans_done;           /*!< Callback function for handling the completion of a reception */
} rmt_uart_event_rx_callbacks_t;

/**
 * @brief Create a new RMT UART controller.
 *
 * This function creates a new RMT UART controller. The controller is used to transmit and receive data
 * over a RMT UART controller.
 *
 * @param[in] uart_config Configuration for the RMT UART controller.
 * @param[out] ret_device Handle to the RMT UART controller.
 *
 * @return
 * - `ESP_OK`: RMT UART controller was successfully created.
 * - `ESP_ERR_INVALID_ARG`: Invalid arguments (e.g., null `uart_config` pointer or `ret_device` pointer).
 * - `ESP_ERR_NO_MEM`: Not enough memory to create the RMT UART controller.
 */
esp_err_t rmt_new_uart_device(const rmt_uart_config_t *uart_config, rmt_uart_device_handle_t *ret_device);

/**
 * @brief Transmit data over a RMT UART controller.
 *
 * This function allows the user to transmit data over a RMT UART controller. The data is encoded and
 * transmitted using the RMT UART controller.
 *
 * @param[in] uart_device Handle to the RMT UART controller, which was previously created using
 *                        `rmt_new_uart_device()`.
 * @param[in] data        Pointer to the data to be transmitted.
 * @param[in] size        Size of the data to be transmitted.
 *
 * @return
 * - `ESP_OK`: Data was successfully transmitted.
 * - `ESP_ERR_INVALID_ARG`: Invalid arguments (e.g., null `uart_device` handle or `data` pointer).
 */
esp_err_t rmt_uart_transmit(rmt_uart_device_handle_t uart_device, const uint8_t *data, size_t size);

/**
 * @brief Receive data over a RMT UART controller.
 *
 * This function allows the user to receive data over a RMT UART controller. The data is received and
 * decoded using the RMT UART controller.
 *
 * @param[in] uart_device Handle to the RMT UART controller, which was previously created using
 *                        `rmt_new_uart_device()`.
 * @param[in] buf         Pointer to the buffer to store the received data.
 * @param[in] size        Size of the buffer to store the received data.
 * @param[in] ticks_to_wait Timeout value in ticks.
 *
 * @return
 * - `ESP_OK`: Data was successfully received.
 * - `ESP_ERR_INVALID_ARG`: Invalid arguments (e.g., null `uart_device` handle or `buf` pointer).
 */
esp_err_t rmt_uart_receive(rmt_uart_device_handle_t uart_device, uint8_t *buf, size_t size);

/**
 * @brief Delete a RMT UART controller.
 *
 * This function deletes a RMT UART controller. The controller is used to transmit and receive data
 * over a RMT UART controller.
 *
 * @param[in] uart_device Handle to the RMT UART controller, which was previously created using
 *                        `rmt_new_uart_device()`.
 *
 * @return
 * - `ESP_OK`: RMT UART controller was successfully deleted.
 * - `ESP_ERR_INVALID_ARG`: Invalid arguments (e.g., null `uart_device` handle).
 */
esp_err_t rmt_delete_uart_device(rmt_uart_device_handle_t uart_device);

/**
 * @brief Decode RMT UART data.
 *
 * This function decodes RMT UART data. The data is decoded using the RMT UART controller.
 *
 * @param[in] uart_device Handle to the RMT UART controller, which was previously created using
 *                        `rmt_new_uart_device()`.
 * @param[in] rmt_symbols Pointer to the RMT symbols to be decoded.
 * @param[in] symbol_num  Number of RMT symbols to be decoded.
 * @param[in] rx_buf      Pointer to the buffer to store the decoded data.
 * @param[in] rx_buf_size Size of the buffer to store the decoded data.
 * @param[in] continue_on_error Whether to continue decoding on error.
 * @return
 *     - (0) Error
 *     - OTHERS (>0) The number of bytes decoded from RMT UART received symbols
 */
int rmt_uart_decode_data(rmt_uart_device_handle_t uart_device, rmt_uart_rx_done_event_data_t *rmt_rx_evt_data, uint8_t *rx_buf, size_t rx_buf_size, bool continue_on_error);

/**
 * @brief Register tx event callback functions for a RMT UART controller.
 *
 * This function allows the user to register callback functions to handle specific RMT UART events, such as
 * transmission or reception completion. The callbacks provide a mechanism to handle asynchronous events
 * generated by the RMT UART controller.
 *
 * @param[in] uart_device Handle to the RMT UART controller, which was previously created using
 *                        `rmt_new_uart_device()`.
 * @param[in] cbs         Pointer to a `rmt_uart_event_rx_callbacks_t` structure that defines the callback
 *                        functions to be registered. This structure includes pointers to the callback
 *                        functions for handling RMT UART events.
 * @param[in] user_data   Pointer to user-defined data that will be passed to the callback functions
 *                        when they are invoked. This can be used to provide context or state information
 *                        specific to the application.
 *
 * @return
 * - `ESP_OK`: Event callbacks were successfully registered.
 * - `ESP_ERR_INVALID_ARG`: Invalid arguments (e.g., null `uart_device` handle or `cbs` pointer).
 */
esp_err_t rmt_uart_register_rx_event_callbacks(rmt_uart_device_handle_t uart_device, const rmt_uart_event_rx_callbacks_t *cbs, void *user_data);

/**
 * @brief Register rx event callback functions for a RMT UART controller.
 *
 * This function allows the user to register callback functions to handle specific RMT UART events, such as
 * transmission or reception completion. The callbacks provide a mechanism to handle asynchronous events
 * generated by the RMT UART controller.
 *
 * @param[in] uart_device Handle to the RMT UART controller, which was previously created using
 *                        `rmt_new_uart_device()`.
 * @param[in] cbs         Pointer to a `rmt_uart_event_tx_callbacks_t` structure that defines the callback
 *                        functions to be registered. This structure includes pointers to the callback
 *                        functions for handling RMT UART events.
 * @param[in] user_data   Pointer to user-defined data that will be passed to the callback functions
 *                        when they are invoked. This can be used to provide context or state information
 *                        specific to the application.
 *
 * @return
 * - `ESP_OK`: Event callbacks were successfully registered.
 * - `ESP_ERR_INVALID_ARG`: Invalid arguments (e.g., null `uart_device` handle or `cbs` pointer).
 */
esp_err_t rmt_uart_register_tx_event_callbacks(rmt_uart_device_handle_t uart_device, const rmt_uart_event_tx_callbacks_t *cbs, void *user_data);

#ifdef __cplusplus
}
#endif
