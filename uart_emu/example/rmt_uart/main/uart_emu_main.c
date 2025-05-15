/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_check.h"
#include "rmt_uart.h"

#define READ_BUFFER_SIZE        128     // in bytes
#define RX_PINGPONG_BUFFER_SIZE 256     // in rmt symbol words
#define EXAMPLE_TX_PIN  1
#define EXAMPLE_RX_PIN  0

const char *TAG = "rmt_uart_example";

IRAM_ATTR static bool rmt_uart_tx_event_cbs(rmt_uart_device_handle_t tx_unit, const rmt_uart_tx_done_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_wakeup = pdFALSE;
    TaskHandle_t task = (TaskHandle_t)user_ctx;
    vTaskNotifyGiveFromISR(task, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

IRAM_ATTR static bool rmt_uart_rx_event_cbs(rmt_uart_device_handle_t rx_unit, const rmt_uart_rx_done_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t receive_queue = (QueueHandle_t)user_ctx;
    xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

void uart_read_task(void *pvParameters)
{
    rmt_uart_device_handle_t uart_device = (rmt_uart_device_handle_t)pvParameters;
    QueueHandle_t receive_queue = xQueueCreate(10, sizeof(rmt_uart_rx_done_event_data_t));   // queue for receive rmt rx done event
    assert(receive_queue != NULL);
    rmt_uart_event_rx_callbacks_t uart_rx_cbs = {
        .on_rx_trans_done = rmt_uart_rx_event_cbs,
    };
    ESP_ERROR_CHECK(rmt_uart_register_rx_event_callbacks(uart_device, &uart_rx_cbs, receive_queue));
    rmt_uart_rx_done_event_data_t rx_done_event_data;
    rmt_symbol_word_t *receive_buffer = heap_caps_calloc(RX_PINGPONG_BUFFER_SIZE, sizeof(rmt_symbol_word_t),
                                        MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    assert(receive_buffer != NULL);

    char read_buffer[READ_BUFFER_SIZE] = {0};
    while (1) {
        /* Read from the UART */
        ESP_ERROR_CHECK(uart_emu_receive(uart_device, (uint8_t *)receive_buffer, RX_PINGPONG_BUFFER_SIZE * sizeof(rmt_symbol_word_t)));
        if (xQueueReceive(receive_queue, &rx_done_event_data, portMAX_DELAY)) {
            int read_len = rmt_uart_decode_data(uart_device, &rx_done_event_data, (uint8_t *)read_buffer, sizeof(read_buffer), true);
            if (read_len > 0) {
                ESP_LOGI(TAG, "Read len: %d, data: %s", read_len, read_buffer);
                memset(read_buffer, 0, sizeof(read_buffer));
            }
        }
    }
}

void app_main(void)
{
    rmt_uart_config_t config = {
        .tx_io_num = EXAMPLE_TX_PIN,
        .rx_io_num = EXAMPLE_RX_PIN,
        .baud_rate = 115200,
        .data_bits = UART_EMU_DATA_8_BITS,
        .stop_bits = UART_EMU_STOP_BITS_1,
        .parity = UART_EMU_PARITY_DISABLE,
        .tx_trans_queue_depth = 10,
        .rx_buffer_size = READ_BUFFER_SIZE,
#if SOC_RMT_SUPPORT_DMA
        .flags.with_dma = true,
#endif
    };
    rmt_uart_device_handle_t uart_device = NULL;
    /* Initialize and configure the software UART port */
    ESP_ERROR_CHECK(rmt_new_uart_device(&config, &uart_device));
    rmt_uart_event_tx_callbacks_t uart_tx_cbs = {
        .on_tx_trans_done = rmt_uart_tx_event_cbs,
    };
    ESP_ERROR_CHECK(rmt_uart_register_tx_event_callbacks(uart_device, &uart_tx_cbs, xTaskGetCurrentTaskHandle()));

    // create a task to read data from the UART
    xTaskCreate(uart_read_task, "uTask", 4096, uart_device, 4, NULL);

    char sendbuf[] = "RMT UART, transmission! RMT UART, transmission! RMT UART, transmission! RMT UART, transmission! RMT UART, transmission! RMT UART, transmission! RMT UART, transmission!";
    for (int i = 0; i < 16; i++) {
        /* Write few bytes to the UART */
        ESP_ERROR_CHECK(uart_emu_transmit(uart_device, (uint8_t *)sendbuf, strlen(sendbuf) + 1));
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    int count = 0;
    while (1) {
        if (!ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(50))) {
            break;
        }
        count++;
    }
    ESP_LOGI(TAG, "UART transmit %d times!", count);

}
