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
#include "uart_emu_rmt.h"

// the memory size of each RMT channel, in words (4 bytes)
#if SOC_RMT_SUPPORT_DMA
#define UART_EMU_RMT_MEM_BLOCK_SYMBOLS   1024
#else
#define UART_EMU_RMT_MEM_BLOCK_SYMBOLS   SOC_RMT_MEM_WORDS_PER_CHANNEL
#endif

#define UART_EMU_RMT_INTR_PRIORITY 3                // use higher priority to avoid timing issues
#define UART_EMU_RMT_TX_TRANS_QUEUE_DEPTH 10        // the max number of the pending tx transaction
#define UART_EMU_RX_BUFFER_SIZE 128                 // the max size of the received data, in bytes
#define UART_EMU_RMT_RX_PINGPONG_BUFFER_SIZE 256    // in rmt symbol words, the size of the ping-pong buffer
#define UART_EMU_TX_PIN  GPIO_NUM_1
#define UART_EMU_RX_PIN  GPIO_NUM_0
#define UART_EMU_BAUD_RATE 115200

const char *TAG = "uart_emu_rmt_example";

IRAM_ATTR static bool uart_emu_rmt_tx_event_cbs(uart_emu_device_handle_t tx_unit, const uart_emu_tx_done_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_wakeup = pdFALSE;
    TaskHandle_t task = (TaskHandle_t)user_ctx;
    vTaskNotifyGiveFromISR(task, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

IRAM_ATTR static bool uart_emu_rmt_rx_event_cbs(uart_emu_device_handle_t rx_unit, const uart_emu_rx_done_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t receive_queue = (QueueHandle_t)user_ctx;
    xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

void uart_read_task(void *pvParameters)
{
    uart_emu_device_handle_t uart_device = (uart_emu_device_handle_t)pvParameters;
    QueueHandle_t receive_queue = xQueueCreate(10, sizeof(uart_emu_rx_done_event_data_t));   // queue for receive rmt rx done event
    assert(receive_queue != NULL);
    uart_emu_event_rx_callbacks_t uart_rx_cbs = {
        .on_rx_trans_done = uart_emu_rmt_rx_event_cbs,
    };
    ESP_ERROR_CHECK(uart_emu_register_rx_event_callbacks(uart_device, &uart_rx_cbs, receive_queue));
    uart_emu_rx_done_event_data_t rx_done_event_data;
    rmt_symbol_word_t *receive_symbols = heap_caps_calloc(UART_EMU_RMT_RX_PINGPONG_BUFFER_SIZE, sizeof(rmt_symbol_word_t),
                                         MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    assert(receive_symbols != NULL);

    char read_buffer[UART_EMU_RX_BUFFER_SIZE] = {0};
    while (1) {
        /* Read from the UART */
        ESP_ERROR_CHECK(uart_emu_receive(uart_device, (uint8_t *)receive_symbols, UART_EMU_RMT_RX_PINGPONG_BUFFER_SIZE * sizeof(rmt_symbol_word_t)));
        if (xQueueReceive(receive_queue, &rx_done_event_data, portMAX_DELAY)) {
            // decode the received symbols to data
            int read_len = uart_emu_rmt_decode_data(uart_device, &rx_done_event_data, (uint8_t *)read_buffer, sizeof(read_buffer), true);
            if (read_len > 0) {
                ESP_LOGI(TAG, "Read len: %d, data: %s", read_len, read_buffer);
                memset(read_buffer, 0, sizeof(read_buffer));
            }
        }
    }
}

void app_main(void)
{
    uart_emu_config_t uart_config = {
        .tx_io_num = UART_EMU_TX_PIN,
        .rx_io_num = UART_EMU_RX_PIN,
        .baud_rate = UART_EMU_BAUD_RATE,
        .data_bits = UART_EMU_DATA_8_BITS,
        .stop_bits = UART_EMU_STOP_BITS_1,
        .parity = UART_EMU_PARITY_DISABLE,
        .rx_buffer_size = UART_EMU_RX_BUFFER_SIZE,
    };
    uart_emu_rmt_config_t rmt_config = {
        .tx_trans_queue_depth = UART_EMU_RMT_TX_TRANS_QUEUE_DEPTH,
        .tx_mem_block_symbols = UART_EMU_RMT_MEM_BLOCK_SYMBOLS,
        .rx_mem_block_symbols = UART_EMU_RMT_MEM_BLOCK_SYMBOLS,
        .intr_priority = UART_EMU_RMT_INTR_PRIORITY,
#if SOC_RMT_SUPPORT_DMA
        .flags.with_dma = true,
#endif
    };
    uart_emu_device_handle_t uart_device = NULL;
    /* Initialize and configure the software UART port */
    ESP_ERROR_CHECK(uart_emu_new_from_rmt(&uart_config, &rmt_config, &uart_device));
    uart_emu_event_tx_callbacks_t uart_tx_cbs = {
        .on_tx_trans_done = uart_emu_rmt_tx_event_cbs,
    };
    ESP_ERROR_CHECK(uart_emu_register_tx_event_callbacks(uart_device, &uart_tx_cbs, xTaskGetCurrentTaskHandle()));

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
