/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "xymodem.h"
#include "example_transport.h"
#include "sdkconfig.h"

#define TAG "xmodem_example"

/* Number of payload bytes to send. Change this to try different transfer sizes. */
#define EXAMPLE_XYMODEM_PAYLOAD_LEN  2048

#if CONFIG_EXAMPLE_XYMODEM_ROLE_SEND
static uint8_t payload[EXAMPLE_XYMODEM_PAYLOAD_LEN];

static void run_send(const xymodem_transport_t *transport)
{
    for (size_t i = 0; i < sizeof(payload); i++) {
        payload[i] = (uint8_t)i;
    }

    const xymodem_send_config_t config = XYMODEM_SEND_CONFIG_DEFAULT();

    ESP_LOGI(TAG, "Sending %u bytes (0, 1, 2, ...)",
             (unsigned)sizeof(payload));
    esp_err_t err = xmodem_send_buffer(transport, &config, payload, sizeof(payload));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Send finished");
    } else {
        ESP_LOGE(TAG, "Send failed: %s", esp_err_to_name(err));
    }
}
#endif

#if CONFIG_EXAMPLE_XYMODEM_ROLE_RECV
static esp_err_t print_write(void *ctx, const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t *total = (size_t *)ctx;
    if (total) {
        *total += len;
    }

    for (size_t i = 0; i < len; i++) {
        printf("%02X%s", p[i], ((i + 1) % 16 == 0) ? "\n" : " ");
    }
    if (len % 16 != 0) {
        printf("\n");
    }
    fflush(stdout);
    return ESP_OK;
}

static void run_recv(const xymodem_transport_t *transport)
{
    size_t total = 0;
    const xmodem_data_sink_t sink = {
        .write = print_write,
        .ctx = &total,
    };
    const xymodem_recv_config_t config = XYMODEM_RECV_CONFIG_DEFAULT();

    ESP_LOGI(TAG, "Receiving. Starting handshake.");
    esp_err_t err = xmodem_recv(transport, &config, &sink);
    printf("\n");
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Receive finished, %u bytes delivered to the sink (includes padding)", (unsigned)total);
    } else {
        ESP_LOGE(TAG, "Receive failed: %s", esp_err_to_name(err));
    }
}
#endif

void app_main(void)
{
    xymodem_transport_t transport = {0};
    example_transport_init(&transport);

    ESP_LOGI(TAG, "UART%d TX=%d RX=%d baud=%d backend=%s role=%s",
             CONFIG_EXAMPLE_XYMODEM_UART_PORT,
             CONFIG_EXAMPLE_XYMODEM_UART_TX_GPIO,
             CONFIG_EXAMPLE_XYMODEM_UART_RX_GPIO,
             CONFIG_EXAMPLE_XYMODEM_UART_BAUD,
#if CONFIG_EXAMPLE_XYMODEM_BACKEND_UHCI
             "UHCI",
#else
             "UART",
#endif
#if CONFIG_EXAMPLE_XYMODEM_ROLE_SEND
             "send"
#else
             "recv"
#endif
            );

    printf("Press 's' to start...\n");
    while (getchar() != 's') {
        vTaskDelay(1);
    }

#if CONFIG_EXAMPLE_XYMODEM_ROLE_SEND
    run_send(&transport);
#else
    run_recv(&transport);
#endif

    example_transport_deinit();
}
