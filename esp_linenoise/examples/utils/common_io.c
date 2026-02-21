/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "common_io.h"

#if !CONFIG_IDF_TARGET_LINUX
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_vfs_dev.h"
#include "esp_log.h"

static int s_uart_fd = -1;
#endif // !CONFIG_IDF_TARGET_LINUX

// init UART and VFS, and set up stdin/stdout to use UART0 as POSIX fd
void common_init_io(void)
{
#ifndef CONFIG_IDF_TARGET_LINUX
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);
    uart_vfs_dev_use_driver(UART_NUM_0);
    s_uart_fd = fileno(stdin); // stdin now routed to UART0
#endif
}

// Deinit UART and VFS routing used by esp_linenoise examples
void common_deinit_io(void)
{
#ifndef CONFIG_IDF_TARGET_LINUX
    if (s_uart_fd >= 0) {
        uart_vfs_dev_use_nonblocking(UART_NUM_0);
        uart_driver_delete(UART_NUM_0);
        s_uart_fd = -1;
    }
#endif
}

// Return a POSIX fd for UART (ESP-IDF only)
int common_open_uart_fd(void)
{
#ifndef CONFIG_IDF_TARGET_LINUX
    if (s_uart_fd < 0) {
        common_init_io();
    }
    return s_uart_fd;
#else
    return fileno(stdin);
#endif
}

// Portable input fd for linenoise
int common_get_default_in_fd(void)
{
#ifndef CONFIG_IDF_TARGET_LINUX
    return common_open_uart_fd();
#else
    return fileno(stdin);
#endif
}

// Portable output fd for linenoise
int common_get_default_out_fd(void)
{
#ifndef CONFIG_IDF_TARGET_LINUX
    return common_open_uart_fd();
#else
    return fileno(stdout);
#endif
}
