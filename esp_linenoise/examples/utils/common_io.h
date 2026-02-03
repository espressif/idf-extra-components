/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize common I/O functionality.
 *
 * This function performs the necessary initialization for common I/O operations.
 * It should be called before using any I/O related functions in the application.
 */
void common_init_io(void);

/**
 * @brief Deinitialize common I/O functionality.
 *
 * This function performs the necessary cleanup for common I/O operations.
 * It should be called when the application is done using any I/O related functions.
 */
void common_deinit_io(void);

/**
 * @brief Return a POSIX fd for UART (ESP-IDF only)
 */
int common_open_uart_fd(void);

/**
 * @brief Return a portable input fd for linenoise
 */
int common_get_default_in_fd(void);

/**
 * @brief Return a portable output fd for linenoise
 */
int common_get_default_out_fd(void);

#ifdef __cplusplus
}
#endif
