/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "onewire_bus_impl_uart.h"
#include "onewire_bus_interface.h"

static const char *TAG = "1-wire.uart";

#define ONEWIRE_UART_DEFAULT_TIMEOUT_MS    100
// refer to https://www.analog.com/en/resources/technical-articles/using-a-uart-to-implement-a-1wire-bus-master.html for more information
#define ONEWIRE_UART_BAUD_RESET            9600   // baud rate for reset pulse and presence detect
#define ONEWIRE_UART_BAUD_SLOT             115200 // baud rate for read and write operations

#define ONEWIRE_UART_RESET_TX              0xF0 // TX value for reset pulse and presence detect
#define ONEWIRE_UART_RESET_RX_NO_DEVICE    0xF0 // RX value when no device is present

#define ONEWIRE_UART_SLOT_TX_WRITE_1       0xFF // TX value for write 1
#define ONEWIRE_UART_SLOT_TX_WRITE_0       0x00 // TX value for write 0
#define ONEWIRE_UART_SLOT_TX_READ          0xFF // TX value for read
#define ONEWIRE_UART_SLOT_RX_READ_1        0xFF // RX value when read 1

typedef struct {
    onewire_bus_t base; /*!< base class */
    uart_port_t uart_port_num; /*!< UART port number */
    gpio_num_t data_gpio_num; /*!< GPIO number for 1-wire bus */
    uint32_t current_baud_rate; /*!< Note: the baud rate returned by uart_get_baudrate() could have a slight deviation from the user-configured baud rate.
                                     That's why we store the configured baud rate here. */
    SemaphoreHandle_t bus_mutex;
} onewire_bus_uart_obj_t;

static esp_err_t onewire_bus_uart_read_bit(onewire_bus_handle_t bus, uint8_t *rx_bit);
static esp_err_t onewire_bus_uart_write_bit(onewire_bus_handle_t bus, uint8_t tx_bit);
static esp_err_t onewire_bus_uart_read_bytes(onewire_bus_handle_t bus, uint8_t *rx_buf, size_t rx_buf_size);
static esp_err_t onewire_bus_uart_write_bytes(onewire_bus_handle_t bus, const uint8_t *tx_data, uint8_t tx_data_size);
static esp_err_t onewire_bus_uart_reset(onewire_bus_handle_t bus);
static esp_err_t onewire_bus_uart_del(onewire_bus_handle_t bus);

static esp_err_t onewire_bus_uart_destroy(onewire_bus_uart_obj_t *bus_uart);
static esp_err_t onewire_bus_uart_set_baud_rate(onewire_bus_uart_obj_t *bus_uart, uint32_t baud_rate);
static esp_err_t onewire_bus_uart_exchange_byte(onewire_bus_uart_obj_t *bus_uart, uint8_t tx_data, uint8_t *rx_data);
static esp_err_t onewire_bus_uart_write_bit_nolock(onewire_bus_uart_obj_t *bus_uart, uint8_t tx_bit);
static esp_err_t onewire_bus_uart_read_bit_nolock(onewire_bus_uart_obj_t *bus_uart, uint8_t *rx_bit);

esp_err_t onewire_new_bus_uart(const onewire_bus_config_t *bus_config, const onewire_bus_uart_config_t *uart_config, onewire_bus_handle_t *ret_bus)
{
    esp_err_t ret = ESP_OK;
    onewire_bus_uart_obj_t *bus_uart = NULL;
    ESP_RETURN_ON_FALSE(bus_config && uart_config && ret_bus, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    bus_uart = calloc(1, sizeof(onewire_bus_uart_obj_t));
    ESP_RETURN_ON_FALSE(bus_uart, ESP_ERR_NO_MEM, TAG, "no mem for onewire_bus_uart_obj_t");
    bus_uart->uart_port_num = UART_NUM_MAX;
    bus_uart->data_gpio_num = GPIO_NUM_NC;

    const gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << bus_config->bus_gpio_num),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = bus_config->flags.en_pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "gpio config failed");
    bus_uart->data_gpio_num = bus_config->bus_gpio_num;

    const uart_config_t uart_cfg = {
        .baud_rate = ONEWIRE_UART_BAUD_RESET,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_GOTO_ON_ERROR(uart_param_config(uart_config->uart_port_num, &uart_cfg), err, TAG, "uart param config failed");
    bus_uart->current_baud_rate = uart_cfg.baud_rate;
    ESP_GOTO_ON_ERROR(uart_set_pin(uart_config->uart_port_num, bus_config->bus_gpio_num, bus_config->bus_gpio_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                      err, TAG, "uart set pin failed");
    ESP_GOTO_ON_ERROR(uart_driver_install(uart_config->uart_port_num, UART_HW_FIFO_LEN(uart_config->uart_port_num) + 1, 0, 0, NULL, 0),
                      err, TAG, "uart driver install failed");
    bus_uart->uart_port_num = uart_config->uart_port_num;

    // Configuration optimized for this scenario
    ESP_GOTO_ON_ERROR(uart_set_rx_full_threshold(uart_config->uart_port_num, 1), err, TAG, "uart set rx full threshold failed");
    ESP_GOTO_ON_ERROR(uart_set_rx_timeout(uart_config->uart_port_num, 1), err, TAG, "uart set rx timeout failed");

    bus_uart->bus_mutex = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(bus_uart->bus_mutex, ESP_ERR_NO_MEM, err, TAG, "bus mutex creation failed");

    bus_uart->base.del = onewire_bus_uart_del;
    bus_uart->base.reset = onewire_bus_uart_reset;
    bus_uart->base.write_bit = onewire_bus_uart_write_bit;
    bus_uart->base.write_bytes = onewire_bus_uart_write_bytes;
    bus_uart->base.read_bit = onewire_bus_uart_read_bit;
    bus_uart->base.read_bytes = onewire_bus_uart_read_bytes;
    *ret_bus = &bus_uart->base;
    return ret;

err:
    if (bus_uart) {
        onewire_bus_uart_destroy(bus_uart);
    }
    return ret;
}

static esp_err_t onewire_bus_uart_destroy(onewire_bus_uart_obj_t *bus_uart)
{
    if (bus_uart->bus_mutex) {
        vSemaphoreDelete(bus_uart->bus_mutex);
    }
    if (bus_uart->uart_port_num != UART_NUM_MAX) {
        uart_driver_delete(bus_uart->uart_port_num);
    }
    if (bus_uart->data_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(bus_uart->data_gpio_num);
    }
    free(bus_uart);
    return ESP_OK;
}

static esp_err_t onewire_bus_uart_set_baud_rate(onewire_bus_uart_obj_t *bus_uart, uint32_t baud_rate)
{
    if (bus_uart->current_baud_rate == baud_rate) {
        return ESP_OK;
    }
    esp_err_t ret = uart_set_baudrate(bus_uart->uart_port_num, baud_rate);
    if (ret != ESP_OK) {
        return ret;
    }
    bus_uart->current_baud_rate = baud_rate;
    return ESP_OK;
}

/**
 * @brief Send and receive one byte over the UART bus.
 *
 * @note This function is used for:
 *   - reset pulse and presence detect
 *   - write or read one bit on the 1-wire bus
 */
static esp_err_t onewire_bus_uart_exchange_byte(onewire_bus_uart_obj_t *bus_uart, uint8_t tx_data, uint8_t *rx_data)
{
    esp_err_t ret = uart_flush_input(bus_uart->uart_port_num);
    if (ret != ESP_OK) {
        return ret;
    }
    if (uart_tx_chars(bus_uart->uart_port_num, (const char *)&tx_data, 1) != 1) {
        return ESP_FAIL;
    }
    if (uart_read_bytes(bus_uart->uart_port_num, rx_data, 1, pdMS_TO_TICKS(ONEWIRE_UART_DEFAULT_TIMEOUT_MS)) != 1) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static esp_err_t onewire_bus_uart_write_bit_nolock(onewire_bus_uart_obj_t *bus_uart, uint8_t tx_bit)
{
    uint8_t rx_data = 0;
    const uint8_t tx_data = tx_bit ? ONEWIRE_UART_SLOT_TX_WRITE_1 : ONEWIRE_UART_SLOT_TX_WRITE_0;
    esp_err_t ret = onewire_bus_uart_exchange_byte(bus_uart, tx_data, &rx_data);
    if (ret != ESP_OK) {
        return ret;
    }
    return (rx_data == tx_data) ? ESP_OK : ESP_ERR_INVALID_STATE;   // Check if the sent data is corrupted
}

static esp_err_t onewire_bus_uart_read_bit_nolock(onewire_bus_uart_obj_t *bus_uart, uint8_t *rx_bit)
{
    uint8_t rx_data = 0;
    esp_err_t ret = onewire_bus_uart_exchange_byte(bus_uart, ONEWIRE_UART_SLOT_TX_READ, &rx_data);
    if (ret != ESP_OK) {
        return ret;
    }
    *rx_bit = (rx_data == ONEWIRE_UART_SLOT_RX_READ_1) ? 1 : 0;
    return ESP_OK;
}

////////////////////////////// implementation of onewire_bus_t functions //////////////////////////////

static esp_err_t onewire_bus_uart_del(onewire_bus_handle_t bus)
{
    onewire_bus_uart_obj_t *bus_uart = __containerof(bus, onewire_bus_uart_obj_t, base);
    return onewire_bus_uart_destroy(bus_uart);
}

static esp_err_t onewire_bus_uart_reset(onewire_bus_handle_t bus)
{
    onewire_bus_uart_obj_t *bus_uart = __containerof(bus, onewire_bus_uart_obj_t, base);
    esp_err_t ret = ESP_OK;
    uint8_t rx_data = 0;

    xSemaphoreTake(bus_uart->bus_mutex, portMAX_DELAY);
    ESP_GOTO_ON_ERROR(onewire_bus_uart_set_baud_rate(bus_uart, ONEWIRE_UART_BAUD_RESET), err, TAG, "set reset baudrate failed");
    ESP_GOTO_ON_ERROR(onewire_bus_uart_exchange_byte(bus_uart, ONEWIRE_UART_RESET_TX, &rx_data), err, TAG, "create reset pulse failed");
    ESP_GOTO_ON_FALSE(rx_data != ONEWIRE_UART_RESET_RX_NO_DEVICE, ESP_ERR_NOT_FOUND, err, TAG, "no 1-wire device found");

err:
    xSemaphoreGive(bus_uart->bus_mutex);
    return ret;
}

static esp_err_t onewire_bus_uart_write_bytes(onewire_bus_handle_t bus, const uint8_t *tx_data, uint8_t tx_data_size)
{
    onewire_bus_uart_obj_t *bus_uart = __containerof(bus, onewire_bus_uart_obj_t, base);
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(tx_data && tx_data_size, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    xSemaphoreTake(bus_uart->bus_mutex, portMAX_DELAY);
    ESP_GOTO_ON_ERROR(onewire_bus_uart_set_baud_rate(bus_uart, ONEWIRE_UART_BAUD_SLOT), err, TAG, "set slot baudrate failed");

    for (uint8_t i = 0; i < tx_data_size; i++) {
        uint8_t current = tx_data[i];
        for (int bit = 0; bit < 8; bit++) {
            ESP_GOTO_ON_ERROR(onewire_bus_uart_write_bit_nolock(bus_uart, current & 0x01), err, TAG, "write bit failed");
            current >>= 1;
        }
    }

err:
    xSemaphoreGive(bus_uart->bus_mutex);
    return ret;
}

static esp_err_t onewire_bus_uart_read_bytes(onewire_bus_handle_t bus, uint8_t *rx_buf, size_t rx_buf_size)
{
    onewire_bus_uart_obj_t *bus_uart = __containerof(bus, onewire_bus_uart_obj_t, base);
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(rx_buf && rx_buf_size, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    memset(rx_buf, 0, rx_buf_size);
    xSemaphoreTake(bus_uart->bus_mutex, portMAX_DELAY);
    ESP_GOTO_ON_ERROR(onewire_bus_uart_set_baud_rate(bus_uart, ONEWIRE_UART_BAUD_SLOT), err, TAG, "set slot baudrate failed");

    for (size_t i = 0; i < rx_buf_size; i++) {
        uint8_t current = 0;
        for (int bit = 0; bit < 8; bit++) {
            uint8_t rx_bit = 0;
            ESP_GOTO_ON_ERROR(onewire_bus_uart_read_bit_nolock(bus_uart, &rx_bit), err, TAG, "read bit failed");
            if (rx_bit) {
                current |= (1 << bit);
            }
        }
        rx_buf[i] = current;
    }

err:
    xSemaphoreGive(bus_uart->bus_mutex);
    return ret;
}

static esp_err_t onewire_bus_uart_write_bit(onewire_bus_handle_t bus, uint8_t tx_bit)
{
    onewire_bus_uart_obj_t *bus_uart = __containerof(bus, onewire_bus_uart_obj_t, base);
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(bus_uart->bus_mutex, portMAX_DELAY);
    ret = onewire_bus_uart_set_baud_rate(bus_uart, ONEWIRE_UART_BAUD_SLOT);
    if (ret == ESP_OK) {
        ret = onewire_bus_uart_write_bit_nolock(bus_uart, tx_bit);
    }
    xSemaphoreGive(bus_uart->bus_mutex);
    return ret;
}

static esp_err_t onewire_bus_uart_read_bit(onewire_bus_handle_t bus, uint8_t *rx_bit)
{
    onewire_bus_uart_obj_t *bus_uart = __containerof(bus, onewire_bus_uart_obj_t, base);
    ESP_RETURN_ON_FALSE(rx_bit, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(bus_uart->bus_mutex, portMAX_DELAY);
    ret = onewire_bus_uart_set_baud_rate(bus_uart, ONEWIRE_UART_BAUD_SLOT);
    if (ret == ESP_OK) {
        ret = onewire_bus_uart_read_bit_nolock(bus_uart, rx_bit);
    }
    xSemaphoreGive(bus_uart->bus_mutex);
    return ret;
}

