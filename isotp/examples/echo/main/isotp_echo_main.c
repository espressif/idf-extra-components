/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "esp_isotp.h"

static const char *TAG = "isotp_echo";

// Global variables for cleanup
static esp_isotp_handle_t g_isotp_handle = NULL;
static twai_node_handle_t g_twai_node = NULL;
static QueueHandle_t g_rx_queue = NULL;

static esp_err_t isotp_echo_init(void);
static esp_err_t isotp_echo_deinit(void);

void app_main(void)
{
    ESP_LOGI(TAG, "ISO-TP Echo Demo started");

    esp_err_t ret = isotp_echo_init();
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to initialize ISO-TP echo: %s", esp_err_to_name(ret));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    ESP_LOGI(TAG, "ISO-TP Echo Demo finished");

err:
    isotp_echo_deinit();
}

static void echo_task(void *arg)
{
    esp_isotp_handle_t isotp_handle = (esp_isotp_handle_t)arg;

    ESP_LOGI(TAG, "ISO-TP Echo task started");

    while (1) {
        esp_isotp_poll(isotp_handle);
        int len = esp_isotp_receive(isotp_handle, isotp_handle->rx_buffer,
                                    CONFIG_EXAMPLE_BUFFER_SIZE);

        if (len > 0) {
            ESP_LOGI(TAG, "Received %u bytes, echoing back...", len);

            int ret = esp_isotp_send(isotp_handle, isotp_handle->rx_buffer, len);

            if (ret == ISOTP_RET_OK) {
                ESP_LOGI(TAG, "Echo sent successfully!");
            } else {
                ESP_LOGE(TAG, "Failed to send echo data, error: %d", ret);
            }
        } else if (len < 0) {
            ESP_LOGE(TAG, "Failed to receive data, error: %d", len);
        }

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
            .quanta_clk_out = -1,
            .bus_off_indicator = -1,
        },
        .clk_src = 0,
        .bit_timing = {
            .bitrate = CONFIG_EXAMPLE_BITRATE,
            .sp_permill = 0,
            .ssp_permill = 0,
        },
        .data_timing = {0},
        .fail_retry_cnt = -1,
        .tx_queue_depth = CONFIG_EXAMPLE_TWAI_TX_QUEUE_DEPTH,
        .intr_priority = 0,
        .flags = {
            .enable_self_test = false,
            .enable_loopback = false,
            .enable_listen_only = false,
            .no_receive_rtr = false,
        },
    };

    esp_err_t ret = twai_new_node_onchip(&twai_cfg, &g_twai_node);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to create TWAI node: %s", esp_err_to_name(ret));

    // Create ISO-TP's queue
    g_rx_queue = xQueueCreate(CONFIG_EXAMPLE_ISOTP_RX_QUEUE_SIZE, sizeof(isotp_rx_queue_item_t));
    ESP_GOTO_ON_FALSE(g_rx_queue != NULL, ESP_ERR_NO_MEM, err, TAG, "Failed to create RX queue");

    esp_isotp_config_t isotp_cfg = {
        .tx_id = CONFIG_EXAMPLE_ISOTP_TX_ID,
        .rx_id = CONFIG_EXAMPLE_ISOTP_RX_ID,
        .tx_buffer_size = CONFIG_EXAMPLE_ISOTP_TX_BUFFER_SIZE,
        .rx_buffer_size = CONFIG_EXAMPLE_ISOTP_RX_BUFFER_SIZE,
        .twai_node = g_twai_node,
        .rx_queue = g_rx_queue,
    };

    g_isotp_handle = esp_isotp_new(&isotp_cfg);
    ESP_GOTO_ON_FALSE(g_isotp_handle != NULL, ESP_ERR_NO_MEM, err, TAG, "Failed to create ISO-TP link");

    // Create poll task
    BaseType_t task_ret = xTaskCreate(echo_task, "isotp_poll", CONFIG_EXAMPLE_ECHO_TASK_STACK_SIZE, g_isotp_handle, CONFIG_EXAMPLE_ECHO_TASK_PRIORITY, NULL);
    ESP_GOTO_ON_FALSE(task_ret == pdPASS, ESP_ERR_NO_MEM, err, TAG, "Failed to create echo task");

    ESP_LOGI(TAG, "ISO-TP echo example's TX ID: 0x%X, RX ID: 0x%X",
             CONFIG_EXAMPLE_ISOTP_TX_ID, CONFIG_EXAMPLE_ISOTP_RX_ID);

    return ESP_OK;

err:
    return ESP_FAIL;
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

    if (g_rx_queue) {
        vQueueDelete(g_rx_queue);
        g_rx_queue = NULL;
    }

    ESP_LOGI(TAG, "ISO-TP echo example deinitialized");
    return ESP_OK;
}
