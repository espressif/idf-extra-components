/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "uart_emu_rmt_encoder.h"

static const char *TAG = "uart_encoder";

#define RMT_BIT_RESOLUTION 8

typedef struct {
    rmt_encoder_t base;           // the base "class", declares the standard encoder interface
    rmt_encoder_t *copy_encoder;  // use the copy_encoder to encode the start and stop bit
    rmt_encoder_t *bytes_encoder; // use the bytes_encoder to encode the data bits
    rmt_symbol_word_t uart_start_symbol; // uart start bit with RMT representation
    rmt_symbol_word_t uart_stop_symbol;  // uart stop bit with RMT representation
    int state;
    int byte_index;               // index of the encoding byte in the primary stream
} uart_emu_rmt_encoder_t;

static size_t rmt_encode_uart(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    uart_emu_rmt_encoder_t *uart_encoder = __containerof(encoder, uart_emu_rmt_encoder_t, base);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    uint8_t *data = (uint8_t *)primary_data;
    int *byte_index = &uart_encoder->byte_index;
    rmt_encoder_handle_t copy_encoder = uart_encoder->copy_encoder;
    rmt_encoder_handle_t bytes_encoder = uart_encoder->bytes_encoder;

    while (*byte_index < data_size) {
        switch (uart_encoder->state) {
        case 0: // send start bit
            encoded_symbols += copy_encoder->encode(copy_encoder, channel, &uart_encoder->uart_start_symbol,
                                                    sizeof(rmt_symbol_word_t), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                uart_encoder->state = 1; // we can only switch to next state when current encoder finished
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                goto out; // yield if there's no free space to put other encoding artifacts
            }
        // fall-through
        case 1: // send data bits
            encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, data + *byte_index, sizeof(uint8_t), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                uart_encoder->state = 2; // we can only switch to next state when current encoder finished
                (*byte_index)++;
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                goto out; // yield if there's no free space to put other encoding artifacts
            }
        // fall-through
        case 2: // send ending code
            encoded_symbols += copy_encoder->encode(copy_encoder, channel, &uart_encoder->uart_stop_symbol,
                                                    sizeof(rmt_symbol_word_t), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                uart_encoder->state = RMT_ENCODING_RESET; // back to the initial encoding session

            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                goto out; // yield if there's no free space to put other encoding artifacts
            }
        }
    }

    // all bytes are encoded, set the state to complete
    *byte_index = 0;
    state |= RMT_ENCODING_COMPLETE;

out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_del_uart_encoder(rmt_encoder_t *encoder)
{
    uart_emu_rmt_encoder_t *uart_encoder = __containerof(encoder, uart_emu_rmt_encoder_t, base);
    rmt_del_encoder(uart_encoder->copy_encoder);
    rmt_del_encoder(uart_encoder->bytes_encoder);
    free(uart_encoder);
    return ESP_OK;
}

static esp_err_t rmt_uart_encoder_reset(rmt_encoder_t *encoder)
{
    uart_emu_rmt_encoder_t *uart_encoder = __containerof(encoder, uart_emu_rmt_encoder_t, base);
    rmt_encoder_reset(uart_encoder->copy_encoder);
    rmt_encoder_reset(uart_encoder->bytes_encoder);
    uart_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

esp_err_t uart_emu_rmt_new_encoder(const uart_emu_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    uart_emu_rmt_encoder_t *uart_encoder = NULL;
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    uart_encoder = rmt_alloc_encoder_mem(sizeof(uart_emu_rmt_encoder_t));
    ESP_GOTO_ON_FALSE(uart_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for uart encoder");
    uart_encoder->base.encode = rmt_encode_uart;
    uart_encoder->base.del = rmt_del_uart_encoder;
    uart_encoder->base.reset = rmt_uart_encoder_reset;

    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &uart_encoder->copy_encoder), err, TAG, "create copy encoder failed");

    // construct the leading code and ending code with RMT symbol format
    uart_encoder->uart_start_symbol = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = RMT_BIT_RESOLUTION / 2,
        .level1 = 0,
        .duration1 = RMT_BIT_RESOLUTION / 2,
    };
    rmt_symbol_word_t stop_bit_symbol = {
        .level0 = 1,
        .level1 = 1,
    };
    switch (config->stop_bits) {
    case UART_EMU_STOP_BITS_1:
        stop_bit_symbol.duration0 = RMT_BIT_RESOLUTION / 2;
        stop_bit_symbol.duration1 = RMT_BIT_RESOLUTION / 2;
        break;
    case UART_EMU_STOP_BITS_2:
        stop_bit_symbol.duration0 = RMT_BIT_RESOLUTION;
        stop_bit_symbol.duration1 = RMT_BIT_RESOLUTION;
        break;
    }
    uart_encoder->uart_stop_symbol = stop_bit_symbol;

    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0 = 0,
            .duration0 = RMT_BIT_RESOLUTION / 2,
            .level1 = 0,
            .duration1 = RMT_BIT_RESOLUTION / 2,
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = RMT_BIT_RESOLUTION / 2,
            .level1 = 1,
            .duration1 = RMT_BIT_RESOLUTION / 2,
        },
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &uart_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");

    *ret_encoder = &uart_encoder->base;
    return ESP_OK;
err:
    if (uart_encoder) {
        if (uart_encoder->bytes_encoder) {
            rmt_del_encoder(uart_encoder->bytes_encoder);
        }
        if (uart_encoder->copy_encoder) {
            rmt_del_encoder(uart_encoder->copy_encoder);
        }
        free(uart_encoder);
    }
    return ret;
}