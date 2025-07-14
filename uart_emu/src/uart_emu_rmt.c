/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "uart_emu_rmt.h"
#include "uart_emu_rmt_encoder.h"
#include "esp_memory_utils.h"

static const char *TAG = "uart_emu_rmt";

// in order to prevent data loss and achieve a higher baudrate, we use 8 RMT clock cycles to represent a UART bit
#define RMT_BIT_RESOLUTION 8

#define ROUND_CLOSEST(dividend, divisor) (((dividend) + ((divisor) / 2)) / (divisor))

typedef struct uart_emu_rmt_tx_context_t uart_emu_rmt_tx_context_t;
typedef struct uart_emu_rmt_rx_context_t uart_emu_rmt_rx_context_t;
typedef struct uart_emu_rmt_decode_context_t uart_emu_rmt_decode_context_t;
typedef struct uart_emu_device_t uart_emu_device_t;

struct uart_emu_rmt_tx_context_t {
    rmt_channel_handle_t tx_channel;                    // rmt tx channel handler
    rmt_encoder_handle_t tx_encoder;                    // rmt tx encoder handle
    rmt_transmit_config_t tx_config;                    // rmt tx config
    uart_emu_tx_done_callback_t on_tx_trans_done;       // callback function for tx done event
    void *user_ctx;                                     // user context for tx done callback
};

struct uart_emu_rmt_rx_context_t {
    rmt_channel_handle_t rx_channel;                    // rmt rx channel handler
    rmt_symbol_word_t *rx_symbols_buf;                  // hold rmt raw symbols
    size_t max_bytes_len;                               // buffer size in byte for single transaction
    uart_emu_rx_done_callback_t on_rx_trans_done;       // callback function for rx done event
    void *user_ctx;                                     // user context for rx done callback
    uint32_t read_symbol_index;                         // the index of the read buffer
};

struct uart_emu_rmt_decode_context_t {
    size_t bit_pos;         // the current bit position
    uint16_t raw_data;      // the raw data being assembled
    size_t byte_pos;        // the decoded byte position
    bool continue_on_error; // whether to continue decoding on error
};

struct uart_emu_device_t {
    uart_emu_rmt_tx_context_t rmt_uart_context_tx;  // rmt tx context
    uart_emu_rmt_rx_context_t rmt_uart_context_rx;  // rmt rx context
    uart_emu_word_length_t data_bits;               // UART byte size
    uart_emu_parity_t parity;                       // UART parity mode
    uart_emu_stop_bits_t stop_bits;                 // UART stop bits
    size_t frame_len;                               // frame length
    uint32_t baud_rate;                             // baud rate
};

static bool uart_emu_rmt_process_level(uint8_t level, uint32_t duration, int bit_ticks,
                                       const uart_emu_device_t *uart_device,
                                       uart_emu_rmt_decode_context_t *decode_context,
                                       uint8_t *rx_buf, size_t rx_buf_size)
{
    int bit_count = ROUND_CLOSEST(duration, bit_ticks);
    uint8_t data_bits = uart_device->data_bits;
    uint8_t stop_bits = (uart_device->stop_bits == UART_EMU_STOP_BITS_1) ? 1 : 2;
    uint8_t total_bits = 1 + data_bits + stop_bits;

    for (int i = 0; i < bit_count; i++) {
        // if the current bit is not a start bit, stop the decoding
        if (decode_context->bit_pos == 0 && level != 0) {
            if (decode_context->continue_on_error) {
                continue;
            } else {
                ESP_LOGE(TAG, "Invalid start bit @ byte %d", decode_context->byte_pos);
                return false;
            }
        }

        decode_context->raw_data |= (level << decode_context->bit_pos);
        decode_context->bit_pos++;

        if (decode_context->bit_pos == total_bits) {
            // Total bits layout:
            // [0] Start bit (always 0)
            // [1 ~ N] N bits data
            // [N + 1] Parity bit (optional)
            // [N + 1 + (1 or 2)] Stop bit (always 1)

            // extract the data byte
            uint8_t data_byte = (decode_context->raw_data >> 1) & ((1 << data_bits) - 1);

            // if the stop bit is wrong, stop the decoding
            if ((decode_context->raw_data >> (total_bits - 1)) != 1) {
                if (decode_context->continue_on_error == false) {
                    ESP_LOGE(TAG, "Invalid stop bit @ byte %d", decode_context->byte_pos);
                }
                decode_context->bit_pos = 0;
                decode_context->raw_data = 0;
                return decode_context->continue_on_error;
            }

            // if the byte position is less than the buffer size, store the data byte
            if (decode_context->byte_pos < rx_buf_size) {
                rx_buf[decode_context->byte_pos++] = data_byte;
                ESP_LOGD(TAG, "Decoded byte[%d] = 0x%02X", decode_context->byte_pos - 1, data_byte);
            }

            decode_context->bit_pos = 0;
            decode_context->raw_data = 0;

            // if the byte position is greater than the buffer size, truncate the data, stop the decoding
            if (decode_context->byte_pos >= rx_buf_size) {
                return false;
            }
        }
    }

    return true;  // continue decoding
}

int uart_emu_rmt_decode_data(uart_emu_device_handle_t uart_device, uart_emu_rx_done_event_data_t *evt_data,
                             uint8_t *rx_buf, size_t rx_buf_size, bool continue_on_error)
{
    if (!uart_device || !evt_data || !rx_buf || rx_buf_size == 0) {
        ESP_LOGE(TAG, "Invalid arguments");
        return 0;
    }

    if (rx_buf_size > uart_device->rmt_uart_context_rx.max_bytes_len) {
        ESP_LOGE(TAG, "rx_buf_size %d should not be greater than %d, which is configured in uart_emu_new_from_rmt", rx_buf_size, uart_device->rmt_uart_context_rx.max_bytes_len);
        return 0;
    }
    rmt_rx_done_event_data_t *rmt_rx_evt_data = (rmt_rx_done_event_data_t *)evt_data->rx_done_event_data;
    uart_emu_rmt_decode_context_t decode_context = {
        .bit_pos = 0,
        .raw_data = 0,
        .byte_pos = 0,
        .continue_on_error = continue_on_error,
    };

    rmt_symbol_word_t *symbols = rmt_rx_evt_data->received_symbols;
    size_t num_symbols = rmt_rx_evt_data->num_symbols;

    for (size_t i = 0; i < num_symbols; i++) {
        rmt_symbol_word_t *sym = &symbols[i];

        ESP_LOGV(TAG, "Symbol[%02d]: duration0: %d level0: %d  duration1: %d level1: %d",
                 i, sym->duration0, sym->level0, sym->duration1, sym->level1);

        // if the symbol is the last one, the duration of the stop bit will be 0 because the rmt_rx_done_event_data is triggered
        // so we need to set the duration of the stop bit manually
        if (i == num_symbols - 1) {
            if (sym->level0 == 1) {
                sym->duration0 = (uart_device->stop_bits + 1) * RMT_BIT_RESOLUTION;
            } else if (sym->level1 == 1) {
                sym->duration1 = (uart_device->stop_bits + 1) * RMT_BIT_RESOLUTION;
            }
        }

        if (!uart_emu_rmt_process_level(sym->level0, sym->duration0, RMT_BIT_RESOLUTION, uart_device, &decode_context, rx_buf, rx_buf_size)) {
            break;
        }
        if (!uart_emu_rmt_process_level(sym->level1, sym->duration1, RMT_BIT_RESOLUTION, uart_device, &decode_context, rx_buf, rx_buf_size)) {
            break;
        }
    }

    return decode_context.byte_pos;
}

IRAM_ATTR bool uart_emu_rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_woken = pdFALSE;
    uart_emu_device_t *uart_device = (uart_emu_device_t *)user_data;
    uart_emu_rmt_rx_context_t *rx_context = &uart_device->rmt_uart_context_rx;
    uint32_t *read_symbol_index = &rx_context->read_symbol_index;

    // avoid memory trampling
    if (*read_symbol_index + edata->num_symbols > rx_context->max_bytes_len * uart_device->frame_len) {
        ESP_EARLY_LOGW(TAG, "Received symbols number is over the buffer size, truncate the data");
        *read_symbol_index = 0;
    }

    // do memory copy, the pingpong buffer should not be very large, so the memory copy should be fast
    if (edata->flags.is_last) {
        memcpy(rx_context->rx_symbols_buf + *read_symbol_index, edata->received_symbols, edata->num_symbols * sizeof(rmt_symbol_word_t));
        *read_symbol_index += edata->num_symbols;
        rmt_rx_done_event_data_t last_edata = {
            .received_symbols = rx_context->rx_symbols_buf,
            .num_symbols = *read_symbol_index,
            .flags.is_last = true,
        };
        uart_emu_rx_done_event_data_t evt_data = {
            .rx_done_event_data = (void *) &last_edata,
        };
        if (rx_context->on_rx_trans_done(uart_device, &evt_data, rx_context->user_ctx)) {
            high_task_woken |= pdTRUE;
        }
        *read_symbol_index = 0;
    } else {
        memcpy(rx_context->rx_symbols_buf + *read_symbol_index, edata->received_symbols, edata->num_symbols * sizeof(rmt_symbol_word_t));
        *read_symbol_index += edata->num_symbols;
    }

    return high_task_woken;
}

IRAM_ATTR bool uart_emu_rmt_tx_done_callback(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_woken = pdFALSE;
    uart_emu_device_t *uart_device = (uart_emu_device_t *)user_data;
    uart_emu_rmt_tx_context_t *tx_context = &uart_device->rmt_uart_context_tx;

    if (tx_context->on_tx_trans_done(uart_device, NULL, tx_context->user_ctx)) {
        high_task_woken |= pdTRUE;
    }
    return high_task_woken;
}

esp_err_t uart_emu_new_from_rmt(const uart_emu_config_t *uart_config, const uart_emu_rmt_config_t *rmt_config, uart_emu_device_handle_t *ret_device)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(uart_config && rmt_config, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    // malloc channel memory
    uart_emu_device_t *uart_device = heap_caps_calloc(1, sizeof(uart_emu_device_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(uart_device, ESP_ERR_NO_MEM, TAG, "no mem for uart_emu_device");
    ESP_RETURN_ON_FALSE(uart_config->data_bits == UART_EMU_DATA_8_BITS, ESP_ERR_INVALID_ARG, TAG, "Invalid data bits");

    uart_device->baud_rate = uart_config->baud_rate;
    uart_device->data_bits = uart_config->data_bits;
    uart_device->stop_bits = uart_config->stop_bits;
    uart_device->parity = uart_config->parity;
    const int resolution = uart_config->baud_rate * RMT_BIT_RESOLUTION;

    // 1 RMT symbol represents 1 bit. 1 bit start and 1 bit stop, n bits data, 1 bit parity (optional), total n + 2 (+ 1) bits, equivalent to n + 2 (+ 1) symbols
    uart_device->frame_len = uart_config->parity ? uart_config->data_bits + 3 : uart_config->data_bits + 2;

    if (uart_config->rx_io_num != GPIO_NUM_NC) {
        uart_emu_rmt_rx_context_t *rx_context = &uart_device->rmt_uart_context_rx;
        rmt_rx_channel_config_t uart_rx_channel_cfg = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = resolution,
            .gpio_num = uart_config->rx_io_num,
            .mem_block_symbols = rmt_config->rx_mem_block_symbols,
            .intr_priority = rmt_config->intr_priority,
            .flags.invert_in = false,
            .flags.with_dma = rmt_config->flags.with_dma,
        };
        ESP_GOTO_ON_ERROR(rmt_new_rx_channel(&uart_rx_channel_cfg, &rx_context->rx_channel), err, TAG, "new rx channel failed");

        rx_context->max_bytes_len = uart_config->rx_buffer_size;

        // allocate rmt rx symbol buffer
        rx_context->rx_symbols_buf = heap_caps_calloc(1, rx_context->max_bytes_len * sizeof(rmt_symbol_word_t) * uart_device->frame_len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        ESP_GOTO_ON_FALSE(rx_context->rx_symbols_buf, ESP_ERR_NO_MEM, err, TAG, "no mem to store received RMT symbols");

        // register rmt rx done callback
        rmt_rx_event_callbacks_t rx_cbs = {
            .on_recv_done = uart_emu_rmt_rx_done_callback
        };
        rmt_rx_register_event_callbacks(rx_context->rx_channel, &rx_cbs, uart_device);

        // enable rmt channels
        rmt_enable(rx_context->rx_channel);
    }

    if (uart_config->tx_io_num != GPIO_NUM_NC) {
        uart_emu_rmt_tx_context_t *tx_context = &uart_device->rmt_uart_context_tx;
        rmt_tx_channel_config_t uart_tx_channel_cfg = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .gpio_num = uart_config->tx_io_num,
            .mem_block_symbols = rmt_config->tx_mem_block_symbols,
            .resolution_hz = resolution,
            .trans_queue_depth = rmt_config->tx_trans_queue_depth,
            .intr_priority = rmt_config->intr_priority,
            .flags.invert_out = false,
            .flags.with_dma = rmt_config->flags.with_dma,
        };
        ESP_GOTO_ON_ERROR(rmt_new_tx_channel(&uart_tx_channel_cfg, &tx_context->tx_channel), err, TAG, "new tx channel failed");

        // IDLE should be high level
        tx_context->tx_config.flags.eot_level = 1;

        ESP_RETURN_ON_ERROR(uart_emu_rmt_new_encoder(uart_config, &tx_context->tx_encoder), TAG, "new encoder failed");

        // register rmt tx done callback
        rmt_tx_event_callbacks_t tx_cbs = {
            .on_trans_done = uart_emu_rmt_tx_done_callback
        };
        rmt_tx_register_event_callbacks(tx_context->tx_channel, &tx_cbs, uart_device);

        // enable rmt channels
        rmt_enable(tx_context->tx_channel);
    }
    *ret_device = uart_device;
    ESP_LOGI(TAG, "new uart emu at %p, baud=%d  rmt_resolution=%d", uart_device, uart_config->baud_rate, resolution);
    return ESP_OK;

err:
    if (uart_device->rmt_uart_context_rx.rx_symbols_buf) {
        free(uart_device->rmt_uart_context_rx.rx_symbols_buf);
    }
    if (uart_device) {
        free(uart_device);
    }
    return ret;
}

esp_err_t uart_emu_transmit(uart_emu_device_handle_t uart_device, const uint8_t *data, size_t size)
{
    uart_emu_rmt_tx_context_t *tx_context = &uart_device->rmt_uart_context_tx;
    ESP_RETURN_ON_FALSE(uart_device, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    // transmit data with the encoder
    ESP_RETURN_ON_ERROR(rmt_transmit(tx_context->tx_channel, tx_context->tx_encoder, data, size, &tx_context->tx_config), TAG, "uart emu transmit failed");

    return ESP_OK;
}

esp_err_t uart_emu_receive(uart_emu_device_handle_t uart_device, uint8_t *buf, size_t rx_buf_size)
{
    uart_emu_rmt_rx_context_t *rx_context = &uart_device->rmt_uart_context_rx;
    ESP_RETURN_ON_FALSE(uart_device, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    // calculate the bit time in nanoseconds
    uint32_t bit_time_ns = 1000000000 / uart_device->baud_rate;
    rmt_receive_config_t receive_config = {
        .signal_range_min_ns = bit_time_ns / 100,
        .signal_range_max_ns = bit_time_ns * 10,
        .flags.en_partial_rx = 1, // the uart data may be large, so we need to enable partial receive to do the pingpong
    };

    rmt_receive(rx_context->rx_channel, buf, rx_buf_size, &receive_config);
    return ESP_OK;
}

esp_err_t uart_emu_delete(uart_emu_device_handle_t uart_device)
{
    ESP_RETURN_ON_FALSE(uart_device, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    // delete RMT TX
    uart_emu_rmt_tx_context_t *tx_context = &uart_device->rmt_uart_context_tx;

    if (tx_context->tx_channel) {
        rmt_disable(tx_context->tx_channel);
        rmt_del_channel(tx_context->tx_channel);
    }

    if (tx_context->tx_encoder) {
        rmt_del_encoder(tx_context->tx_encoder);
    }

    // delete RMT RX
    uart_emu_rmt_rx_context_t *rx_context = &uart_device->rmt_uart_context_rx;

    if (rx_context->rx_channel) {
        rmt_disable(rx_context->rx_channel);
        rmt_del_channel(rx_context->rx_channel);
    }

    // delete uart device
    if (rx_context->rx_symbols_buf) {
        free(rx_context->rx_symbols_buf);
    }

    free(uart_device);

    return ESP_OK;
}

esp_err_t uart_emu_register_tx_event_callbacks(uart_emu_device_handle_t uart_device, const uart_emu_event_tx_callbacks_t *cbs, void *user_data)
{
    ESP_RETURN_ON_FALSE(uart_device && cbs, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

#if CONFIG_RMT_TX_ISR_CACHE_SAFE
    if (cbs->on_tx_trans_done) {
        ESP_RETURN_ON_FALSE(esp_ptr_in_iram(cbs->on_tx_trans_done), ESP_ERR_INVALID_ARG, TAG, "on_tx_trans_done callback not in IRAM");
    }
    if (user_data) {
        ESP_RETURN_ON_FALSE(esp_ptr_internal(user_data), ESP_ERR_INVALID_ARG, TAG, "user context not in internal RAM");
    }
#endif

    uart_device->rmt_uart_context_tx.on_tx_trans_done = cbs->on_tx_trans_done;
    uart_device->rmt_uart_context_tx.user_ctx = user_data;
    return ESP_OK;
}

esp_err_t uart_emu_register_rx_event_callbacks(uart_emu_device_handle_t uart_device, const uart_emu_event_rx_callbacks_t *cbs, void *user_data)
{
    ESP_RETURN_ON_FALSE(uart_device && cbs, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

#if CONFIG_RMT_RX_ISR_CACHE_SAFE
    if (cbs->on_rx_trans_done) {
        ESP_RETURN_ON_FALSE(esp_ptr_in_iram(cbs->on_rx_trans_done), ESP_ERR_INVALID_ARG, TAG, "on_rx_trans_done callback not in IRAM");
    }
    if (user_data) {
        ESP_RETURN_ON_FALSE(esp_ptr_internal(user_data), ESP_ERR_INVALID_ARG, TAG, "user context not in internal RAM");
    }
#endif
    uart_device->rmt_uart_context_rx.on_rx_trans_done = cbs->on_rx_trans_done;
    uart_device->rmt_uart_context_rx.user_ctx = user_data;
    return ESP_OK;
}