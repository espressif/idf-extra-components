/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "xymodem.h"
#include "example_transport.h"
#include "sdkconfig.h"

#define TAG "xmodem_example"

/* Storage for one received XMODEM transfer (includes trailing 0x1A padding). */
#define EXAMPLE_XYMODEM_BUF_SIZE  2048

static uint8_t s_buf[EXAMPLE_XYMODEM_BUF_SIZE];

typedef struct {
    size_t stored;
    size_t offset;
    unsigned packet_n;
} echo_ctx_t;

static esp_err_t echo_write(void *ctx, const void *buf, size_t len)
{
    echo_ctx_t *echo = ctx;
    if (echo->stored + len > sizeof(s_buf)) {
        ESP_LOGE(TAG, "Buffer full (%u + %u > %u)",
                 (unsigned)echo->stored, (unsigned)len, (unsigned)sizeof(s_buf));
        return ESP_ERR_NO_MEM;
    }
    memcpy(s_buf + echo->stored, buf, len);
    echo->stored += len;
    echo->packet_n++;
    ESP_LOGI(TAG, "Received packet %u, %u bytes", echo->packet_n, (unsigned)len);
    return ESP_OK;
}

static esp_err_t echo_read(void *ctx, void *buf, size_t len, size_t *read_len)
{
    echo_ctx_t *echo = ctx;
    size_t remain = echo->stored - echo->offset;
    size_t n = remain < len ? remain : len;
    memcpy(buf, s_buf + echo->offset, n);
    echo->offset += n;
    *read_len = n;
    if (n > 0) {
        echo->packet_n++;
        ESP_LOGI(TAG, "Sending packet %u, %u bytes", echo->packet_n, (unsigned)n);
    }
    return ESP_OK;
}

static void run_echo_once(const xymodem_transport_t *transport)
{
    echo_ctx_t echo = {0};
    const xmodem_data_sink_t sink = {
        .write = echo_write,
        .ctx = &echo,
    };
    const xmodem_data_source_t source = {
        .read = echo_read,
        .ctx = &echo,
    };
    const xymodem_recv_config_t recv_config = XYMODEM_RECV_CONFIG_DEFAULT();
    const xymodem_send_config_t send_config = XYMODEM_SEND_CONFIG_DEFAULT();

    ESP_LOGI(TAG, "Receiving into %u-byte buffer", (unsigned)sizeof(s_buf));
    esp_err_t err = xmodem_recv(transport, &recv_config, &sink);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Receive failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Receive finished, %u bytes", (unsigned)echo.stored);

    echo.offset = 0;
    echo.packet_n = 0;

    ESP_LOGI(TAG, "Sending %u bytes back", (unsigned)echo.stored);
    err = xmodem_send(transport, &send_config, &source);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Send failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Send finished");
}

void app_main(void)
{
    xymodem_transport_t transport = {0};
    example_transport_init(&transport);

    ESP_LOGI(TAG, "UART%d TX=%d RX=%d baud=%d",
             CONFIG_EXAMPLE_XYMODEM_UART_PORT,
             CONFIG_EXAMPLE_XYMODEM_UART_TX_GPIO,
             CONFIG_EXAMPLE_XYMODEM_UART_RX_GPIO,
             CONFIG_EXAMPLE_XYMODEM_UART_BAUD);

    while (1) {
        run_echo_once(&transport);
    }
}
