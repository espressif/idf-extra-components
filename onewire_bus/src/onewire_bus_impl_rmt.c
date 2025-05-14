/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_attr.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "driver/gpio.h"
#include "esp_private/gpio.h"
#include "onewire_bus_impl_rmt.h"
#include "onewire_bus_interface.h"
#include "esp_idf_version.h"

static const char *TAG = "1-wire.rmt";

#define ONEWIRE_RMT_RESOLUTION_HZ               1000000 // RMT channel default resolution for 1-wire bus, 1MHz, 1tick = 1us
#define ONEWIRE_RMT_DEFAULT_TRANS_QUEUE_SIZE    4

// the memory size of each RMT channel, in words (4 bytes)
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define ONEWIRE_RMT_DEFAULT_MEM_BLOCK_SYMBOLS   64
#else
#define ONEWIRE_RMT_DEFAULT_MEM_BLOCK_SYMBOLS   48
#endif

// for chips whose RMT RX channel doesn't support ping-pong, we need the user to tell the maximum number of bytes will be received
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
// one RMT symbol represents one bit, so x8
#define ONEWIRE_RMT_RX_MEM_BLOCK_SIZE           (rmt_config->max_rx_bytes * 8)
#else // otherwise, we just use one memory block, to save resources
#define ONEWIRE_RMT_RX_MEM_BLOCK_SIZE           ONEWIRE_RMT_DEFAULT_MEM_BLOCK_SYMBOLS
#endif

/*
Reset Pulse:

          | RESET_PULSE | RESET_WAIT_DURATION |
          | _DURATION   |                     |
          |             |   | | RESET     |   |
          |             | * | | _PRESENCE |   |
          |             |   | | _DURATION |   |
----------+             +-----+           +--------------
          |             |     |           |
          |             |     |           |
          |             |     |           |
          +-------------+     +-----------+
*: RESET_PRESENCE_WAIT_DURATION
*/
#define ONEWIRE_RESET_PULSE_DURATION            500 // duration of reset bit
#define ONEWIRE_RESET_WAIT_DURATION             200 // how long should master wait for device to show its presence
#define ONEWIRE_RESET_PRESENCE_WAIT_DURATION_MIN 15 // minimum duration for master to wait device to show its presence
#define ONEWIRE_RESET_PRESENCE_DURATION_MIN      60 // minimum duration for master to recognize device as present

/*
Write 1 bit:

          | SLOT_START | SLOT_BIT  | SLOT_RECOVERY | NEXT
          | _DURATION  | _DURATION | _DURATION     | SLOT
          |            |           |               |
----------+            +-------------------------------------
          |            |
          |            |
          |            |
          +------------+

Write 0 bit:

          | SLOT_START | SLOT_BIT  | SLOT_RECOVERY | NEXT
          | _DURATION  | _DURATION | _DURATION     | SLOT
          |            |           |               |
----------+                        +-------------------------
          |                        |
          |                        |
          |                        |
          +------------------------+

Read 1 bit:


          | SLOT_START | SLOT_BIT_DURATION | SLOT_RECOVERY | NEXT
          | _DURATION  |                   | _DURATION     | SLOT
          |            | SLOT_BIT_   |     |               |
          |            | SAMPLE_TIME |     |               |
----------+            +----------------------------------------------
          |            |
          |            |
          |            |
          +------------+

Read 0 bit:

          | SLOT_START | SLOT_BIT_DURATION | SLOT_RECOVERY | NEXT
          | _DURATION  |                   | _DURATION     | SLOT
          |            | SLOT_BIT_   |     |               |
          |            | SAMPLE_TIME |     |               |
----------+            |             |  +-----------------------------
          |            |                |
          |            |   PULLED DOWN  |
          |            |    BY DEVICE   |
          +-----------------------------+
*/
#define ONEWIRE_SLOT_START_DURATION             2  // bit start pulse duration
#define ONEWIRE_SLOT_BIT_DURATION               60 // duration for each bit to transmit
// refer to https://www.maximintegrated.com/en/design/technical-documents/app-notes/3/3829.html for more information
#define ONEWIRE_SLOT_RECOVERY_DURATION          5  // recovery time between each bit, should be longer in parasite power mode
#define ONEWIRE_SLOT_BIT_SAMPLE_TIME            15 // how long after bit start pulse should the master sample from the bus

typedef struct {
    onewire_bus_t base; /*!< base class */
    rmt_channel_handle_t tx_channel; /*!< rmt tx channel handler */
    rmt_channel_handle_t rx_channel; /*!< rmt rx channel handler */

    gpio_num_t data_gpio_num; /*!< GPIO number for 1-wire bus */

    rmt_encoder_handle_t tx_bytes_encoder; /*!< used to encode commands and data */
    rmt_encoder_handle_t tx_copy_encoder; /*!< used to encode reset pulse and bits */

    rmt_symbol_word_t *rx_symbols_buf; /*!< hold rmt raw symbols */

    size_t max_rx_bytes; /*!< buffer size in byte for single receive transaction */

    QueueHandle_t receive_queue;
    SemaphoreHandle_t bus_mutex;
} onewire_bus_rmt_obj_t;

static rmt_symbol_word_t onewire_reset_pulse_symbol = {
    .level0 = 0,
    .duration0 = ONEWIRE_RESET_PULSE_DURATION,
    .level1 = 1,
    .duration1 = ONEWIRE_RESET_WAIT_DURATION
};

static rmt_symbol_word_t onewire_bit0_symbol = {
    .level0 = 0,
    .duration0 = ONEWIRE_SLOT_START_DURATION + ONEWIRE_SLOT_BIT_DURATION,
    .level1 = 1,
    .duration1 = ONEWIRE_SLOT_RECOVERY_DURATION
};

static rmt_symbol_word_t onewire_bit1_symbol = {
    .level0 = 0,
    .duration0 = ONEWIRE_SLOT_START_DURATION,
    .level1 = 1,
    .duration1 = ONEWIRE_SLOT_BIT_DURATION + ONEWIRE_SLOT_RECOVERY_DURATION
};

const static rmt_transmit_config_t onewire_rmt_tx_config = {
    .loop_count = 0,     // no transfer loop
    .flags.eot_level = 1 // onewire bus should be released in IDLE
};

const static rmt_receive_config_t onewire_rmt_rx_config = {
    .signal_range_min_ns = 1000000000 / ONEWIRE_RMT_RESOLUTION_HZ,
    .signal_range_max_ns = (ONEWIRE_RESET_PULSE_DURATION + ONEWIRE_RESET_WAIT_DURATION) * 1000,
};

static esp_err_t onewire_bus_rmt_read_bit(onewire_bus_handle_t bus, uint8_t *rx_bit);
static esp_err_t onewire_bus_rmt_write_bit(onewire_bus_handle_t bus, uint8_t tx_bit);
static esp_err_t onewire_bus_rmt_read_bytes(onewire_bus_handle_t bus, uint8_t *rx_buf, size_t rx_buf_size);
static esp_err_t onewire_bus_rmt_write_bytes(onewire_bus_handle_t bus, const uint8_t *tx_data, uint8_t tx_data_size);
static esp_err_t onewire_bus_rmt_reset(onewire_bus_handle_t bus);
static esp_err_t onewire_bus_rmt_del(onewire_bus_handle_t bus);
static esp_err_t onewire_bus_rmt_destroy(onewire_bus_rmt_obj_t *bus_rmt);

IRAM_ATTR
bool onewire_rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    BaseType_t task_woken = pdFALSE;
    onewire_bus_rmt_obj_t *bus_rmt = (onewire_bus_rmt_obj_t *)user_data;

    xQueueSendFromISR(bus_rmt->receive_queue, edata, &task_woken);

    return task_woken;
}

/*
[0].0 means symbol[0].duration0

First reset pulse after rmt channel init:

Bus is low | Reset | Wait |  Device  |  Bus Idle
after init | Pulse |      | Presence |
                   +------+          +-----------
                   |      |          |
                   |      |          |
                   |      |          |
-------------------+      +----------+
                   1      2          3

          [0].1     [0].0     [1].1     [1].0


Following reset pulses:

Bus is high | Reset | Wait |  Device  |  Bus Idle
after init  | Pulse |      | Presence |
------------+       +------+          +-----------
            |       |      |          |
            |       |      |          |
            |       |      |          |
            +-------+      +----------+
            1       2      3          4

              [0].0  [0].1     [1].0    [1].1
*/
static bool onewire_rmt_check_presence_pulse(rmt_symbol_word_t *rmt_symbols, size_t symbol_num)
{
    bool ret = false;
    if (symbol_num >= 2) { // there should be at lease 2 symbols(3 or 4 edges)
        if (rmt_symbols[0].level1 == 1) { // bus is high before reset pulse
            if (rmt_symbols[0].duration1 > ONEWIRE_RESET_PRESENCE_WAIT_DURATION_MIN &&
                    rmt_symbols[1].duration0 > ONEWIRE_RESET_PRESENCE_DURATION_MIN) {
                ret = true;
            }
        } else { // bus is low before reset pulse(first pulse after rmt channel init)
            if (rmt_symbols[0].duration0 > ONEWIRE_RESET_PRESENCE_WAIT_DURATION_MIN &&
                    rmt_symbols[1].duration1 > ONEWIRE_RESET_PRESENCE_DURATION_MIN) {
                ret = true;
            }
        }
    }
    return ret;
}

static void onewire_rmt_decode_data(rmt_symbol_word_t *rmt_symbols, size_t symbol_num, uint8_t *rx_buf, size_t rx_buf_size)
{
    size_t byte_pos = 0;
    size_t bit_pos = 0;
    for (size_t i = 0; i < symbol_num; i ++) {
        if (rmt_symbols[i].duration0 > ONEWIRE_SLOT_BIT_SAMPLE_TIME) { // 0 bit
            rx_buf[byte_pos] &= ~(1 << bit_pos); // LSB first
        } else { // 1 bit
            rx_buf[byte_pos] |= 1 << bit_pos;
        }
        bit_pos ++;
        if (bit_pos >= 8) {
            bit_pos = 0;
            byte_pos ++;
            if (byte_pos >= rx_buf_size) {
                break;
            }
        }
    }
}

esp_err_t onewire_new_bus_rmt(const onewire_bus_config_t *bus_config, const onewire_bus_rmt_config_t *rmt_config, onewire_bus_handle_t *ret_bus)
{
    esp_err_t ret = ESP_OK;
    onewire_bus_rmt_obj_t *bus_rmt = NULL;
    ESP_RETURN_ON_FALSE(bus_config && rmt_config && ret_bus, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    bus_rmt = calloc(1, sizeof(onewire_bus_rmt_obj_t));
    ESP_RETURN_ON_FALSE(bus_rmt, ESP_ERR_NO_MEM, TAG, "no mem for onewire_bus_rmt_obj_t");
    bus_rmt->data_gpio_num = GPIO_NUM_NC;

    // create rmt bytes encoder to transmit 1-wire commands and data
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = onewire_bit0_symbol,
        .bit1 = onewire_bit1_symbol,
        .flags.msb_first = 0,
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &bus_rmt->tx_bytes_encoder),
                      err, TAG, "create bytes encoder failed");

    // create rmt copy encoder to transmit 1-wire reset pulse or bits
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &bus_rmt->tx_copy_encoder),
                      err, TAG, "create copy encoder failed");

    // create RX and TX channels and bind them to the same GPIO
    rmt_rx_channel_config_t onewire_rx_channel_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = ONEWIRE_RMT_RESOLUTION_HZ,
        .gpio_num = bus_config->bus_gpio_num,
        .mem_block_symbols = ONEWIRE_RMT_RX_MEM_BLOCK_SIZE,
    };
    ESP_GOTO_ON_ERROR(rmt_new_rx_channel(&onewire_rx_channel_cfg, &bus_rmt->rx_channel),
                      err, TAG, "create rmt rx channel failed");

    rmt_tx_channel_config_t onewire_tx_channel_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = ONEWIRE_RMT_RESOLUTION_HZ,
        .gpio_num = bus_config->bus_gpio_num,
        .mem_block_symbols = ONEWIRE_RMT_DEFAULT_MEM_BLOCK_SYMBOLS,
        .trans_queue_depth = ONEWIRE_RMT_DEFAULT_TRANS_QUEUE_SIZE,
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
        .flags.io_loop_back = true,
        .flags.io_od_mode = true,
#endif
    };
    ESP_GOTO_ON_ERROR(rmt_new_tx_channel(&onewire_tx_channel_cfg, &bus_rmt->tx_channel),
                      err, TAG, "create rmt tx channel failed");

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    bus_rmt->data_gpio_num = bus_config->bus_gpio_num;
    // enable open-drain mode for 1-wire bus
    gpio_od_enable(bus_rmt->data_gpio_num);
#endif

    // allocate rmt rx symbol buffer, one RMT symbol represents one bit, so x8
    bus_rmt->rx_symbols_buf = malloc(rmt_config->max_rx_bytes * sizeof(rmt_symbol_word_t) * 8);
    ESP_GOTO_ON_FALSE(bus_rmt->rx_symbols_buf, ESP_ERR_NO_MEM, err, TAG, "no mem to store received RMT symbols");
    bus_rmt->max_rx_bytes = rmt_config->max_rx_bytes;

    bus_rmt->receive_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    ESP_GOTO_ON_FALSE(bus_rmt->receive_queue, ESP_ERR_NO_MEM, err, TAG, "receive queue creation failed");

    bus_rmt->bus_mutex = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(bus_rmt->bus_mutex, ESP_ERR_NO_MEM, err, TAG, "bus mutex creation failed");

    // register rmt rx done callback
    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = onewire_rmt_rx_done_callback
    };
    ESP_GOTO_ON_ERROR(rmt_rx_register_event_callbacks(bus_rmt->rx_channel, &cbs, bus_rmt),
                      err, TAG, "enable rmt rx channel failed");

    // enable rmt channels
    ESP_GOTO_ON_ERROR(rmt_enable(bus_rmt->rx_channel), err, TAG, "enable rmt rx channel failed");
    ESP_GOTO_ON_ERROR(rmt_enable(bus_rmt->tx_channel), err, TAG, "enable rmt tx channel failed");

    // release the bus by sending a special RMT symbol
    static rmt_symbol_word_t release_symbol = {
        .level0 = 1,
        .duration0 = 1,
        .level1 = 1,
        .duration1 = 0,
    };
    ESP_GOTO_ON_ERROR(rmt_transmit(bus_rmt->tx_channel, bus_rmt->tx_copy_encoder, &release_symbol,
                                   sizeof(release_symbol), &onewire_rmt_tx_config), err, TAG, "release bus failed");

    bus_rmt->base.del = onewire_bus_rmt_del;
    bus_rmt->base.reset = onewire_bus_rmt_reset;
    bus_rmt->base.write_bit = onewire_bus_rmt_write_bit;
    bus_rmt->base.write_bytes = onewire_bus_rmt_write_bytes;
    bus_rmt->base.read_bit = onewire_bus_rmt_read_bit;
    bus_rmt->base.read_bytes = onewire_bus_rmt_read_bytes;
    *ret_bus = &bus_rmt->base;

    return ret;

err:
    if (bus_rmt) {
        onewire_bus_rmt_destroy(bus_rmt);
    }

    return ret;
}

static esp_err_t onewire_bus_rmt_destroy(onewire_bus_rmt_obj_t *bus_rmt)
{
    if (bus_rmt->tx_bytes_encoder) {
        rmt_del_encoder(bus_rmt->tx_bytes_encoder);
    }
    if (bus_rmt->tx_copy_encoder) {
        rmt_del_encoder(bus_rmt->tx_copy_encoder);
    }
    if (bus_rmt->rx_channel) {
        rmt_disable(bus_rmt->rx_channel);
        rmt_del_channel(bus_rmt->rx_channel);
    }
    if (bus_rmt->tx_channel) {
        rmt_disable(bus_rmt->tx_channel);
        rmt_del_channel(bus_rmt->tx_channel);
    }
    if (bus_rmt->receive_queue) {
        vQueueDelete(bus_rmt->receive_queue);
    }
    if (bus_rmt->bus_mutex) {
        vSemaphoreDelete(bus_rmt->bus_mutex);
    }
    if (bus_rmt->rx_symbols_buf) {
        free(bus_rmt->rx_symbols_buf);
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    if (bus_rmt->data_gpio_num != GPIO_NUM_NC) {
        gpio_od_disable(bus_rmt->data_gpio_num);
    }
#endif
    free(bus_rmt);
    return ESP_OK;
}

static esp_err_t onewire_bus_rmt_del(onewire_bus_handle_t bus)
{
    onewire_bus_rmt_obj_t *bus_rmt = __containerof(bus, onewire_bus_rmt_obj_t, base);
    return onewire_bus_rmt_destroy(bus_rmt);
}

static esp_err_t onewire_bus_rmt_reset(onewire_bus_handle_t bus)
{
    onewire_bus_rmt_obj_t *bus_rmt = __containerof(bus, onewire_bus_rmt_obj_t, base);
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(bus_rmt->bus_mutex, portMAX_DELAY);
    // send reset pulse while receive presence pulse
    ESP_GOTO_ON_ERROR(rmt_receive(bus_rmt->rx_channel, bus_rmt->rx_symbols_buf, sizeof(rmt_symbol_word_t) * 2, &onewire_rmt_rx_config),
                      err, TAG, "1-wire reset pulse receive failed");
    ESP_GOTO_ON_ERROR(rmt_transmit(bus_rmt->tx_channel, bus_rmt->tx_copy_encoder, &onewire_reset_pulse_symbol, sizeof(onewire_reset_pulse_symbol), &onewire_rmt_tx_config),
                      err, TAG, "1-wire reset pulse transmit failed");

    // wait and check presence pulse
    rmt_rx_done_event_data_t rmt_rx_evt_data;
    ESP_GOTO_ON_FALSE(xQueueReceive(bus_rmt->receive_queue, &rmt_rx_evt_data, pdMS_TO_TICKS(1000)) == pdPASS,
                      ESP_ERR_TIMEOUT, err, TAG, "1-wire reset pulse receive timeout");
    if (onewire_rmt_check_presence_pulse(rmt_rx_evt_data.received_symbols, rmt_rx_evt_data.num_symbols) == false) {
        ret = ESP_ERR_NOT_FOUND;
    }

err:
    xSemaphoreGive(bus_rmt->bus_mutex);
    return ret;
}

static esp_err_t onewire_bus_rmt_write_bytes(onewire_bus_handle_t bus, const uint8_t *tx_data, uint8_t tx_data_size)
{
    onewire_bus_rmt_obj_t *bus_rmt = __containerof(bus, onewire_bus_rmt_obj_t, base);
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(bus_rmt->bus_mutex, portMAX_DELAY);
    // transmit data with the bytes encoder
    ESP_GOTO_ON_ERROR(rmt_transmit(bus_rmt->tx_channel, bus_rmt->tx_bytes_encoder, tx_data, tx_data_size, &onewire_rmt_tx_config),
                      err, TAG, "1-wire data transmit failed");
    // wait the transmission to complete
    ESP_GOTO_ON_ERROR(rmt_tx_wait_all_done(bus_rmt->tx_channel, 50), err, TAG, "wait for 1-wire data transmit failed");

err:
    xSemaphoreGive(bus_rmt->bus_mutex);
    return ret;
}

// While receiving data, we use rmt transmit channel to send 0xFF to generate read pulse,
// at the same time, receive channel is used to record weather the bus is pulled down by device.
static esp_err_t onewire_bus_rmt_read_bytes(onewire_bus_handle_t bus, uint8_t *rx_buf, size_t rx_buf_size)
{
    onewire_bus_rmt_obj_t *bus_rmt = __containerof(bus, onewire_bus_rmt_obj_t, base);
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(rx_buf_size <= bus_rmt->max_rx_bytes, ESP_ERR_INVALID_ARG, TAG, "rx_buf_size too large for buffer to hold");
    memset(rx_buf, 0, rx_buf_size);

    xSemaphoreTake(bus_rmt->bus_mutex, portMAX_DELAY);

    // transmit one bits to generate read clock
    uint8_t tx_buffer[rx_buf_size];
    memset(tx_buffer, 0xFF, rx_buf_size);
    // transmit 1 bits while receiving
    ESP_GOTO_ON_ERROR(rmt_receive(bus_rmt->rx_channel, bus_rmt->rx_symbols_buf, rx_buf_size * 8 * sizeof(rmt_symbol_word_t), &onewire_rmt_rx_config),
                      err, TAG, "1-wire data receive failed");
    ESP_GOTO_ON_ERROR(rmt_transmit(bus_rmt->tx_channel, bus_rmt->tx_bytes_encoder, tx_buffer, sizeof(tx_buffer), &onewire_rmt_tx_config),
                      err, TAG, "1-wire data transmit failed");

    // wait the transmission finishes and decode data
    rmt_rx_done_event_data_t rmt_rx_evt_data;
    ESP_GOTO_ON_FALSE(xQueueReceive(bus_rmt->receive_queue, &rmt_rx_evt_data, pdMS_TO_TICKS(1000)) == pdPASS, ESP_ERR_TIMEOUT,
                      err, TAG, "1-wire data receive timeout");
    onewire_rmt_decode_data(rmt_rx_evt_data.received_symbols, rmt_rx_evt_data.num_symbols, rx_buf, rx_buf_size);

err:
    xSemaphoreGive(bus_rmt->bus_mutex);
    return ret;
}

static esp_err_t onewire_bus_rmt_write_bit(onewire_bus_handle_t bus, uint8_t tx_bit)
{
    onewire_bus_rmt_obj_t *bus_rmt = __containerof(bus, onewire_bus_rmt_obj_t, base);
    const rmt_symbol_word_t *symbol_to_transmit = tx_bit ? &onewire_bit1_symbol : &onewire_bit0_symbol;
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(bus_rmt->bus_mutex, portMAX_DELAY);

    // transmit bit
    ESP_GOTO_ON_ERROR(rmt_transmit(bus_rmt->tx_channel, bus_rmt->tx_copy_encoder, symbol_to_transmit, sizeof(rmt_symbol_word_t), &onewire_rmt_tx_config),
                      err, TAG, "1-wire bit transmit failed");
    // wait the transmission to complete
    ESP_GOTO_ON_ERROR(rmt_tx_wait_all_done(bus_rmt->tx_channel, 50), err, TAG, "wait for 1-wire bit transmit failed");

err:
    xSemaphoreGive(bus_rmt->bus_mutex);
    return ret;
}

static esp_err_t onewire_bus_rmt_read_bit(onewire_bus_handle_t bus, uint8_t *rx_bit)
{
    onewire_bus_rmt_obj_t *bus_rmt = __containerof(bus, onewire_bus_rmt_obj_t, base);
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(bus_rmt->bus_mutex, portMAX_DELAY);

    // transmit 1 bit while receiving
    ESP_GOTO_ON_ERROR(rmt_receive(bus_rmt->rx_channel, bus_rmt->rx_symbols_buf, sizeof(rmt_symbol_word_t), &onewire_rmt_rx_config),
                      err, TAG, "1-wire bit receive failed");
    ESP_GOTO_ON_ERROR(rmt_transmit(bus_rmt->tx_channel, bus_rmt->tx_copy_encoder, &onewire_bit1_symbol, sizeof(rmt_symbol_word_t), &onewire_rmt_tx_config),
                      err, TAG, "1-wire bit transmit failed");

    // wait the transmission finishes and decode data
    rmt_rx_done_event_data_t rmt_rx_evt_data;
    ESP_GOTO_ON_FALSE(xQueueReceive(bus_rmt->receive_queue, &rmt_rx_evt_data, pdMS_TO_TICKS(1000)) == pdPASS, ESP_ERR_TIMEOUT,
                      err, TAG, "1-wire bit receive timeout");
    uint8_t rx_buffer = 0;
    onewire_rmt_decode_data(rmt_rx_evt_data.received_symbols, rmt_rx_evt_data.num_symbols, &rx_buffer, sizeof(rx_buffer));
    *rx_bit = rx_buffer & 0x01;

err:
    xSemaphoreGive(bus_rmt->bus_mutex);
    return ret;
}
