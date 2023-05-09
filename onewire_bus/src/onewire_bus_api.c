/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "esp_check.h"
#include "onewire_types.h"
#include "onewire_bus_interface.h"

static const char *TAG = "1-wire";

esp_err_t onewire_bus_reset(onewire_bus_handle_t bus)
{
    ESP_RETURN_ON_FALSE(bus, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return bus->reset(bus);
}

esp_err_t onewire_bus_write_bytes(onewire_bus_handle_t bus, const uint8_t *tx_data, uint8_t tx_data_size)
{
    ESP_RETURN_ON_FALSE(bus && tx_data && tx_data_size, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return bus->write_bytes(bus, tx_data, tx_data_size);
}

esp_err_t onewire_bus_read_bytes(onewire_bus_handle_t bus, uint8_t *rx_buf, size_t rx_buf_size)
{
    ESP_RETURN_ON_FALSE(bus && rx_buf && rx_buf_size, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return bus->read_bytes(bus, rx_buf, rx_buf_size);
}

esp_err_t onewire_bus_write_bit(onewire_bus_handle_t bus, uint8_t tx_bit)
{
    ESP_RETURN_ON_FALSE(bus, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return bus->write_bit(bus, tx_bit);
}

esp_err_t onewire_bus_read_bit(onewire_bus_handle_t bus, uint8_t *rx_bit)
{
    ESP_RETURN_ON_FALSE(bus && rx_bit, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return bus->read_bit(bus, rx_bit);
}

esp_err_t onewire_bus_del(onewire_bus_handle_t bus)
{
    ESP_RETURN_ON_FALSE(bus, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return bus->del(bus);
}
