/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "isotp-c/isotp_config.h"
#include "esp_isotp.h"

static const char *TAG = "isotp_echo";

// Global variables for cleanup
static esp_isotp_handle_t g_isotp_handle = NULL;
static twai_node_handle_t g_twai_node = NULL;
static esp_err_t isotp_echo_init(void);
static esp_err_t isotp_echo_deinit(void);

#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
static void on_tx_done(void *link, uint32_t tx_size, void *user_arg)
{
    ESP_EARLY_LOGI(TAG, "TX complete: %lu bytes", tx_size);
}
#endif

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
static void on_rx_done(void *link, const uint8_t *data, uint32_t size, void *user_arg)
{
    ESP_EARLY_LOGI(TAG, "RX complete: %lu bytes, echoing back...", size);

    // Echo back the received data immediately using isotp_send
    int ret = isotp_send((IsoTpLink *)link, data, size);
    if (ret != ISOTP_RET_OK) {
        ESP_EARLY_LOGE(TAG, "Echo send failed: %d", ret);
    }
}
#endif

void app_main(void)
{
    ESP_LOGI(TAG, "ISO-TP Echo Demo started");

    // Initialize the ISO-TP echo example
    ESP_ERROR_CHECK(isotp_echo_init());

    // Main task will just sleep, the echo_task handles the ISO-TP communication
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    // Deinitialize the ISO-TP echo example
    isotp_echo_deinit();
}

static void echo_task(void *arg)
{
    esp_isotp_handle_t isotp_handle = (esp_isotp_handle_t)arg;

    ESP_LOGI(TAG, "ISO-TP Echo task started");

    while (1) {
        // Poll ISO-TP protocol state machine (timeouts, consecutive frames, etc.)
        ESP_ERROR_CHECK(esp_isotp_poll(isotp_handle));

        // Small delay to ensure accurate STmin timing and prevent 100% CPU usage
        vTaskDelay(pdMS_TO_TICKS(CONFIG_EXAMPLE_ECHO_POLL_DELAY_MS));
    }

    ESP_LOGI(TAG, "ISO-TP Echo task finished");
    vTaskDelete(NULL);
}

static esp_err_t isotp_echo_init(void)
{
    twai_onchip_node_config_t twai_cfg = {
        .io_cfg = {
            .tx = CONFIG_EXAMPLE_TX_GPIO_NUM,
            .rx = CONFIG_EXAMPLE_RX_GPIO_NUM,
        },
        .bit_timing.bitrate = CONFIG_EXAMPLE_BITRATE,
        .tx_queue_depth = CONFIG_EXAMPLE_TWAI_TX_QUEUE_DEPTH,
        .intr_priority = 0,
    };

    ESP_ERROR_CHECK(twai_new_node_onchip(&twai_cfg, &g_twai_node));

    esp_isotp_config_t isotp_cfg = {
        .tx_id = CONFIG_EXAMPLE_ISOTP_TX_ID,
        .rx_id = CONFIG_EXAMPLE_ISOTP_RX_ID,
        .tx_buffer_size = CONFIG_EXAMPLE_ISOTP_TX_BUFFER_SIZE,
        .rx_buffer_size = CONFIG_EXAMPLE_ISOTP_RX_BUFFER_SIZE,
    };

    ESP_ERROR_CHECK(esp_isotp_new_transport(g_twai_node, &isotp_cfg, &g_isotp_handle));

    // Set callback functions - no need to pass handle, link is provided directly
#ifdef ISO_TP_TRANSMIT_COMPLETE_CALLBACK
    ESP_ERROR_CHECK(esp_isotp_set_tx_done_callback(g_isotp_handle, on_tx_done, NULL));
#endif

#ifdef ISO_TP_RECEIVE_COMPLETE_CALLBACK
    ESP_ERROR_CHECK(esp_isotp_set_rx_done_callback(g_isotp_handle, on_rx_done, NULL));
#endif

    // Create echo task
    BaseType_t task_ret = xTaskCreate(echo_task, "isotp_echo", CONFIG_EXAMPLE_ECHO_TASK_STACK_SIZE,
                                      g_isotp_handle, CONFIG_EXAMPLE_ECHO_TASK_PRIORITY, NULL);
    assert(task_ret == pdPASS && "Failed to create echo task");

    ESP_LOGI(TAG, "ISO-TP echo example's TX ID: 0x%X, RX ID: 0x%X",
             CONFIG_EXAMPLE_ISOTP_TX_ID, CONFIG_EXAMPLE_ISOTP_RX_ID);

    return ESP_OK;
}

static esp_err_t isotp_echo_deinit(void)
{
    ESP_RETURN_ON_FALSE(g_isotp_handle != NULL, ESP_OK, TAG, "ISO-TP echo example is not initialized");

    esp_isotp_delete(g_isotp_handle);
    g_isotp_handle = NULL;

    if (g_twai_node) {
        twai_node_delete(g_twai_node);
        g_twai_node = NULL;
    }

    ESP_LOGI(TAG, "ISO-TP echo example deinitialized");
    return ESP_OK;
}
