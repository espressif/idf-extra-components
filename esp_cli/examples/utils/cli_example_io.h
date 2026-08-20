/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize a dedicated UART for an esp_cli instance.
 *
 * Installs the UART driver on the given port, assigns TX/RX pins,
 * registers a VFS endpoint and returns file descriptors suitable for
 * esp_linenoise in_fd / out_fd.
 *
 * The chosen UART port should NOT be the default console UART
 * (CONFIG_ESP_CONSOLE_UART_NUM) so that ESP-IDF logging output
 * does not interfere with the CLI.
 *
 * On Linux, uart_num/tx/rx are ignored and *in_fd / *out_fd are set
 * to STDIN_FILENO / STDOUT_FILENO.
 *
 * @param[in]  uart_num  UART port number (e.g. UART_NUM_1).
 * @param[in]  tx_pin    GPIO number for TX.
 * @param[in]  rx_pin    GPIO number for RX.
 * @param[out] in_fd     File descriptor for reading (input).
 * @param[out] out_fd    File descriptor for writing (output).
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if in_fd or out_fd is NULL
 *  - Other error codes from UART / VFS APIs
 */
esp_err_t cli_example_init_uart(int uart_num, int tx_pin, int rx_pin,
                                int *in_fd, int *out_fd);

/**
 * @brief De-initialize UART I/O previously set up by cli_example_init_uart().
 *
 * Closes the file descriptor, unregisters the VFS and uninstalls the UART driver.
 *
 * @param[in] uart_num  UART port number passed to cli_example_init_uart().
 * @param[in] fd        File descriptor returned by cli_example_init_uart().
 */
void cli_example_deinit_uart(int uart_num, int fd);

/**
 * @brief Initialize USB Serial JTAG console I/O for an esp_cli instance.
 *
 * Configures the USB Serial JTAG peripheral with VFS integration and returns
 * file descriptors that can be used for esp_linenoise in_fd / out_fd.
 *
 * On Linux this is not available; the function is only compiled for ESP targets.
 *
 * @param[out] in_fd   File descriptor for reading (input).
 * @param[out] out_fd  File descriptor for writing (output).
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if in_fd or out_fd is NULL
 *  - Other error codes from USB Serial JTAG / VFS APIs
 */
esp_err_t cli_example_init_usb_serial_jtag(int *in_fd, int *out_fd);

/**
 * @brief De-initialize USB Serial JTAG I/O previously set up by
 *        cli_example_init_usb_serial_jtag().
 *
 * Closes the file descriptor, unregisters the VFS and uninstalls the driver.
 *
 * @param[in] fd  File descriptor returned by cli_example_init_usb_serial_jtag().
 */
void cli_example_deinit_usb_serial_jtag(int fd);

#ifdef __cplusplus
}
#endif
