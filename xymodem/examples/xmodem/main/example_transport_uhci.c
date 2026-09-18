/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "example_transport.h"
#include "driver/uart.h"
#include "driver/uhci.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

#define EXAMPLE_UART_PORT         ((uart_port_t)CONFIG_EXAMPLE_XYMODEM_UART_PORT)
#define EXAMPLE_UHCI_DMA_SIZE     2048
#define EXAMPLE_UHCI_RINGBUF_SIZE 2048

typedef struct {
    uhci_controller_handle_t uhci;
    uint8_t *dma_rx;
    uint8_t *dma_tx;
    RingbufHandle_t rx_rb;
    volatile bool rx_overflow;
} uhci_transport_ctx_t;

static uhci_transport_ctx_t s_ctx;

IRAM_ATTR static bool uhci_rx_cb(uhci_controller_handle_t uhci_ctrl, const uhci_rx_event_data_t *edata, void *user_ctx)
{
    uhci_transport_ctx_t *ctx = user_ctx;
    BaseType_t woken = pdFALSE;
    if (xRingbufferSendFromISR(ctx->rx_rb, edata->data, edata->recv_size, &woken) != pdTRUE) {
        ctx->rx_overflow = true;
    }
    return woken == pdTRUE;
}

static esp_err_t uhci_transport_recv(void *ctx, void *buf, size_t len, size_t *recv_len, uint32_t timeout_ms)
{
    uhci_transport_ctx_t *uhci_ctx = ctx;
    *recv_len = 0;

    if (uhci_ctx->rx_overflow) {
        return ESP_FAIL;
    }

    const int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;

    while (*recv_len < len) {
        TickType_t wait_ticks;
        if (timeout_ms == 0) {
            wait_ticks = 0;
        } else {
            int64_t now = esp_timer_get_time();
            if (now >= deadline) {
                break;
            }
            uint32_t wait_ms = (uint32_t)((deadline - now + 999) / 1000);
            wait_ticks = pdMS_TO_TICKS(wait_ms) ? : 1;
        }

        size_t n = 0;
        uint8_t *item = (uint8_t *)xRingbufferReceiveUpTo(uhci_ctx->rx_rb, &n, wait_ticks, len - *recv_len);
        if (item == NULL) {
            break;
        }
        memcpy((uint8_t *)buf + *recv_len, item, n);
        vRingbufferReturnItem(uhci_ctx->rx_rb, item);
        *recv_len += n;
    }

    return uhci_ctx->rx_overflow ? ESP_FAIL : ESP_OK;
}

static esp_err_t uhci_transport_send(void *ctx, const void *buf, size_t len)
{
    uhci_transport_ctx_t *uhci_ctx = ctx;
    const uint8_t *src = buf;
    while (len > 0) {
        size_t n = len < EXAMPLE_UHCI_DMA_SIZE ? len : EXAMPLE_UHCI_DMA_SIZE;
        memcpy(uhci_ctx->dma_tx, src, n);
        esp_err_t err = uhci_transmit(uhci_ctx->uhci, uhci_ctx->dma_tx, n);
        if (err != ESP_OK) {
            return err;
        }
        err = uhci_wait_all_tx_transaction_done(uhci_ctx->uhci, 2000);
        if (err != ESP_OK) {
            return err;
        }
        src += n;
        len -= n;
    }
    return ESP_OK;
}

void example_transport_init(xymodem_transport_t *transport)
{
    uart_config_t uart_config = {
        .baud_rate = CONFIG_EXAMPLE_XYMODEM_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(EXAMPLE_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(EXAMPLE_UART_PORT, CONFIG_EXAMPLE_XYMODEM_UART_TX_GPIO,
                                 CONFIG_EXAMPLE_XYMODEM_UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    s_ctx.dma_rx = heap_caps_calloc(1, EXAMPLE_UHCI_DMA_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s_ctx.dma_tx = heap_caps_calloc(1, EXAMPLE_UHCI_DMA_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s_ctx.rx_rb = xRingbufferCreate(EXAMPLE_UHCI_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    ESP_ERROR_CHECK((s_ctx.dma_rx && s_ctx.dma_tx && s_ctx.rx_rb) ? ESP_OK : ESP_ERR_NO_MEM);

    uhci_controller_config_t uhci_cfg = {
        .uart_port = EXAMPLE_UART_PORT,
        .tx_trans_queue_depth = 4,
        .max_receive_internal_mem = EXAMPLE_UHCI_DMA_SIZE,
        .max_transmit_size = EXAMPLE_UHCI_DMA_SIZE,
        .dma_burst_size = 32,
        .rx_eof_flags.idle_eof = 1,
    };
    ESP_ERROR_CHECK(uhci_new_controller(&uhci_cfg, &s_ctx.uhci));

    uhci_event_callbacks_t cbs = {
        .on_rx_trans_event = uhci_rx_cb,
    };
    ESP_ERROR_CHECK(uhci_register_event_callbacks(s_ctx.uhci, &cbs, &s_ctx));
    ESP_ERROR_CHECK(uhci_start_receive_continuous(s_ctx.uhci, s_ctx.dma_rx, EXAMPLE_UHCI_DMA_SIZE));

    transport->recv = uhci_transport_recv;
    transport->send = uhci_transport_send;
    transport->ctx = &s_ctx;
}

void example_transport_deinit(void)
{
    uhci_stop_receive(s_ctx.uhci);
    uhci_del_controller(s_ctx.uhci);
    vRingbufferDelete(s_ctx.rx_rb);
    free(s_ctx.dma_rx);
    free(s_ctx.dma_tx);
}
