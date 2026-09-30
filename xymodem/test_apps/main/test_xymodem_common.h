/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/uart.h"
#include "xymodem.h"
#include "sdkconfig.h"

#define XYMODEM_TEST_SENDER_UART    ((uart_port_t)CONFIG_XYMODEM_TEST_SENDER_UART)
#define XYMODEM_TEST_RECEIVER_UART  ((uart_port_t)CONFIG_XYMODEM_TEST_RECEIVER_UART)

#define XYMODEM_TEST_SEND_TIMEOUT_MS 5000
#define XYMODEM_TEST_RECV_TIMEOUT_MS 1000
#define XYMODEM_TEST_MAX_RETRY_CNT   3

typedef struct {
    uart_port_t port;
} xymodem_test_uart_ctx_t;

typedef struct {
    const uint8_t *data;
    size_t len;
    size_t offset;
} xymodem_test_mem_source_t;

typedef struct {
    uint8_t *data;
    size_t cap;
    size_t len;
} xymodem_test_mem_sink_t;

typedef struct {
    esp_err_t send;
    esp_err_t recv;
} xymodem_test_result_t;

void xymodem_test_uart_init(void);

esp_err_t xymodem_test_recv_uart(void *ctx, void *buf, size_t len, size_t *recv_len, uint32_t timeout_ms);
esp_err_t xymodem_test_send_uart(void *ctx, const void *buf, size_t len);

esp_err_t xymodem_test_read_mem(void *ctx, void *buf, size_t len, size_t *read_len);
esp_err_t xymodem_test_write_mem(void *ctx, const void *buf, size_t len);

void xymodem_test_fill_pattern(uint8_t *buf, size_t len);

/**
 * Run XMODEM send and receive in two tasks, then return both protocol results.
 */
xymodem_test_result_t xymodem_test_run_xmodem(const xymodem_transport_t *send_transport,
                                              const xymodem_transport_t *recv_transport,
                                              xymodem_block_size_t block_size,
                                              xymodem_check_type_t check_type,
                                              const xmodem_data_source_t *source,
                                              const xmodem_data_sink_t *sink);

/**
 * Same as `xymodem_test_run_xmodem()`, but uses `xmodem_send_buffer()` / `xmodem_recv_buffer()`.
 * `recv_len` may be NULL.
 */
xymodem_test_result_t xymodem_test_run_xmodem_buffer(const xymodem_transport_t *send_transport,
                                                     const xymodem_transport_t *recv_transport,
                                                     xymodem_block_size_t block_size,
                                                     xymodem_check_type_t check_type,
                                                     const void *tx_buf, size_t tx_len,
                                                     void *rx_buf, size_t rx_cap,
                                                     size_t *recv_len);
