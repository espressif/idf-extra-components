/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "cli_example_io.h"

#include "esp_log.h"
#include "esp_vfs.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "driver/esp_private/uart_vfs.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "driver/esp_private/usb_serial_jtag_vfs.h"

static const char *TAG = "cli_example_io";

esp_err_t cli_example_init_uart(int uart_num, int tx_pin, int rx_pin,
                                int *in_fd, int *out_fd)
{
    if (!in_fd || !out_fd) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    uart_vfs_dev_port_set_rx_line_endings(uart_num, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    uart_vfs_dev_port_set_tx_line_endings(uart_num, ESP_LINE_ENDINGS_CRLF);

    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
#if SOC_UART_SUPPORT_REF_TICK
        .source_clk = UART_SCLK_REF_TICK,
#elif SOC_UART_SUPPORT_XTAL_CLK
        .source_clk = UART_SCLK_XTAL,
#endif
    };

    esp_err_t ret = uart_driver_install(uart_num, 256, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install (UART%d) failed: %s", uart_num, esp_err_to_name(ret));
        return ret;
    }

    ret = uart_param_config(uart_num, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config (UART%d) failed: %s", uart_num, esp_err_to_name(ret));
        uart_driver_delete(uart_num);
        return ret;
    }

    ret = uart_set_pin(uart_num, tx_pin, rx_pin,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin (UART%d) failed: %s", uart_num, esp_err_to_name(ret));
        uart_driver_delete(uart_num);
        return ret;
    }

    /* Tell VFS to use the UART driver for this port */
    uart_vfs_dev_use_driver(uart_num);

    /* Register VFS at a dedicated path and open an FD for this UART port */
    const esp_vfs_fs_ops_t *uart_vfs = esp_vfs_uart_get_vfs();
    ret = esp_vfs_register_fs("/dev/cli_uart", uart_vfs, 0, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_vfs_register_fs for UART%d failed: %s", uart_num, esp_err_to_name(ret));
        uart_vfs_dev_use_nonblocking(uart_num);
        uart_driver_delete(uart_num);
        return ret;
    }

    /* The number after the path selects the UART port */
    char dev_path[32];
    snprintf(dev_path, sizeof(dev_path), "/dev/cli_uart/%d", uart_num);
    int fd = open(dev_path, 0);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open %s", dev_path);
        esp_vfs_unregister("/dev/cli_uart");
        uart_vfs_dev_use_nonblocking(uart_num);
        uart_driver_delete(uart_num);
        return ESP_FAIL;
    }

    /* Ensure blocking read mode */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }

    *in_fd = fd;
    *out_fd = fd;

    ESP_LOGI(TAG, "UART%d I/O initialized (tx=%d, rx=%d, fd=%d)",
             uart_num, tx_pin, rx_pin, fd);
    return ESP_OK;
}

void cli_example_deinit_uart(int uart_num, int fd)
{
    if (fd >= 0) {
        close(fd);
    }
    esp_vfs_unregister("/dev/cli_uart");
    uart_vfs_dev_use_nonblocking(uart_num);
    uart_driver_delete(uart_num);
    ESP_LOGI(TAG, "UART%d I/O de-initialized", uart_num);
}

esp_err_t cli_example_init_usb_serial_jtag(int *in_fd, int *out_fd)
{
    if (!in_fd || !out_fd) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Configure line endings */
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    /* Install USB Serial JTAG driver */
    usb_serial_jtag_driver_config_t usj_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    esp_err_t ret = usb_serial_jtag_driver_install(&usj_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "usb_serial_jtag_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Switch VFS to use the installed driver for blocking reads */
    usb_serial_jtag_vfs_use_driver();

    /* Register VFS at a dedicated path and open an FD */
    const esp_vfs_fs_ops_t *usj_vfs = esp_vfs_usb_serial_jtag_get_vfs();
    ret = esp_vfs_register_fs("/dev/usj", usj_vfs, 0, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_vfs_register_fs for USJ failed: %s", esp_err_to_name(ret));
        usb_serial_jtag_driver_uninstall();
        return ret;
    }

    int fd = open("/dev/usj/0", 0);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open USB Serial JTAG VFS device");
        esp_vfs_unregister("/dev/usj");
        usb_serial_jtag_driver_uninstall();
        return ESP_FAIL;
    }

    /* Ensure blocking read mode */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }

    *in_fd = fd;
    *out_fd = fd;

    ESP_LOGI(TAG, "USB Serial JTAG I/O initialized (fd=%d)", fd);
    return ESP_OK;
}

void cli_example_deinit_usb_serial_jtag(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
    esp_vfs_unregister("/dev/usj");
    usb_serial_jtag_vfs_use_nonblocking();
    usb_serial_jtag_driver_uninstall();
    ESP_LOGI(TAG, "USB Serial JTAG I/O de-initialized");
}
