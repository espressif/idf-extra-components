/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include "example_transport.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#define TAG "xmodem_example"

#define EXAMPLE_UART_PORT   ((uart_port_t)CONFIG_EXAMPLE_XYMODEM_UART_PORT)
#define EXAMPLE_UART_BUF    2048

#if CONFIG_EXAMPLE_XYMODEM_BACKEND_UART

typedef struct {
    uart_port_t port;
} uart_transport_ctx_t;

static uart_transport_ctx_t s_ctx;

static esp_err_t uart_transport_recv(void *ctx, void *buf, size_t len, size_t *recv_len, uint32_t timeout_ms)
{
    uart_transport_ctx_t *uart = ctx;
    uint32_t ticks_to_wait = pdMS_TO_TICKS(timeout_ms) ? : (timeout_ms ? 1 : 0);
    int n = uart_read_bytes(uart->port, buf, len, ticks_to_wait);
    if (n < 0) {
        return ESP_FAIL;
    }
    *recv_len = (size_t)n;
    return ESP_OK;
}

static esp_err_t uart_transport_send(void *ctx, const void *buf, size_t len)
{
    uart_transport_ctx_t *uart = ctx;
    int n = uart_write_bytes(uart->port, buf, len);
    if (n < 0 || (size_t)n != len) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t example_transport_init(xymodem_transport_t *transport)
{
    uart_config_t uart_config = {
        .baud_rate = CONFIG_EXAMPLE_XYMODEM_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(uart_driver_install(EXAMPLE_UART_PORT, EXAMPLE_UART_BUF, EXAMPLE_UART_BUF, 0, NULL, 0),
                        TAG, "uart_driver_install failed");
    ESP_RETURN_ON_ERROR(uart_param_config(EXAMPLE_UART_PORT, &uart_config),
                        TAG, "uart_param_config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(EXAMPLE_UART_PORT, CONFIG_EXAMPLE_XYMODEM_UART_TX_GPIO,
                                     CONFIG_EXAMPLE_XYMODEM_UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        TAG, "uart_set_pin failed");
    ESP_RETURN_ON_ERROR(uart_flush_input(EXAMPLE_UART_PORT), TAG, "uart_flush_input failed");

    s_ctx.port = EXAMPLE_UART_PORT;
    transport->recv = uart_transport_recv;
    transport->send = uart_transport_send;
    transport->ctx = &s_ctx;
    return ESP_OK;
}

void example_transport_deinit(void)
{
    uart_driver_delete(EXAMPLE_UART_PORT);
}

#endif /* CONFIG_EXAMPLE_XYMODEM_BACKEND_UART */
