/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdbool.h>
#include "esp_check.h"
#include "esp_log.h"
#include "onewire_bus.h"
#include "onewire_device.h"
#include "onewire_crc.h"
#include "onewire_cmd.h"

static const char *TAG = "1-wire.device";

typedef struct onewire_device_iter_t {
    onewire_bus_handle_t bus;
    uint16_t last_discrepancy;
    bool is_last_device;
    uint8_t rom_number[sizeof(onewire_device_address_t)];
} onewire_device_iter_t;

esp_err_t onewire_new_device_iter(onewire_bus_handle_t bus, onewire_device_iter_handle_t *ret_iter)
{
    ESP_RETURN_ON_FALSE(bus && ret_iter, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    onewire_device_iter_t *iter = calloc(1, sizeof(onewire_device_iter_t));
    ESP_RETURN_ON_FALSE(iter, ESP_ERR_NO_MEM, TAG, "no mem for device iterator");

    iter->bus = bus;
    *ret_iter = iter;

    return ESP_OK;
}

esp_err_t onewire_del_device_iter(onewire_device_iter_handle_t iter)
{
    ESP_RETURN_ON_FALSE(iter, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    free(iter);

    return ESP_OK;
}

// Search algorithm inspired by https://www.analog.com/en/app-notes/1wire-search-algorithm.html
esp_err_t onewire_device_iter_get_next(onewire_device_iter_handle_t iter, onewire_device_t *dev)
{
    ESP_RETURN_ON_FALSE(iter && dev, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    // we don't treat iterator ending and ESP_ERR_NOT_FOUND as an error condition, so just print debug message here
    if (iter->is_last_device) {
        ESP_LOGD(TAG, "1-wire rom search finished");
        return ESP_ERR_NOT_FOUND;
    }
    onewire_bus_handle_t bus = iter->bus;
    esp_err_t reset_result = onewire_bus_reset(bus);
    if (reset_result == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "reset bus failed: no devices found");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_RETURN_ON_ERROR(reset_result, TAG, "reset bus failed");

    // send rom search command and start search algorithm
    ESP_RETURN_ON_ERROR(onewire_bus_write_bytes(bus, (uint8_t[]) {
        ONEWIRE_CMD_SEARCH_NORMAL
    }, 1), TAG, "send ONEWIRE_CMD_SEARCH_NORMAL failed");

    uint8_t last_zero = 0;
    for (uint16_t rom_bit_index = 0; rom_bit_index < sizeof(onewire_device_address_t) * 8; rom_bit_index ++) {
        uint8_t rom_byte_index = rom_bit_index / 8;
        uint8_t rom_bit_mask = 1 << (rom_bit_index % 8); // calculate byte index and bit mask in advance for convenience

        uint8_t rom_bit = 0;
        uint8_t rom_bit_complement = 0;
        ESP_RETURN_ON_ERROR(onewire_bus_read_bit(bus, &rom_bit), TAG, "read rom_bit error"); // write 1 bit to read from the bus
        ESP_RETURN_ON_ERROR(onewire_bus_read_bit(bus, &rom_bit_complement), TAG, "read rom_bit_complement error"); // read a bit and its complement

        // No devices participating in search.
        if (rom_bit && rom_bit_complement) {
            ESP_LOGE(TAG, "no devices participating in search");
            return ESP_ERR_NOT_FOUND;
        }

        uint8_t search_direction;
        if (rom_bit != rom_bit_complement) { // There are only 0s or 1s in the bit of the participating ROM numbers.
            search_direction = rom_bit;  // just go ahead
        } else { // There are both 0s and 1s in the current bit position of the participating ROM numbers. This is a discrepancy.
            if (rom_bit_index < iter->last_discrepancy) { // current id bit is before the last discrepancy bit
                search_direction = (iter->rom_number[rom_byte_index] & rom_bit_mask) ? 0x01 : 0x00; // follow previous way
            } else {
                search_direction = (rom_bit_index == iter->last_discrepancy) ? 0x01 : 0x00; // search for 0 bit first
            }

            if (search_direction == 0) { // record zero's position in last zero
                last_zero = rom_bit_index;
            }
        }

        if (search_direction == 1) { // set corresponding rom bit by search direction
            iter->rom_number[rom_byte_index] |= rom_bit_mask;
        } else {
            iter->rom_number[rom_byte_index] &= ~rom_bit_mask;
        }

        // set search direction
        ESP_RETURN_ON_ERROR(onewire_bus_write_bit(bus, search_direction), TAG, "write direction bit error");
    }

    // if the search was successful
    iter->last_discrepancy = last_zero;
    if (iter->last_discrepancy == 0) { // last zero loops back to the first bit
        iter->is_last_device = true;
    }

    // check crc
    ESP_RETURN_ON_FALSE(onewire_crc8(0, iter->rom_number, 7) == iter->rom_number[7], ESP_ERR_INVALID_CRC, TAG, "bad device crc");

    // save the ROM number as the device address
    memcpy(&dev->address, iter->rom_number, sizeof(onewire_device_address_t));
    dev->bus = bus;
    ESP_LOGD(TAG, "new 1-Wire device found, address: %016llX", dev->address);

    return ESP_OK;
}
