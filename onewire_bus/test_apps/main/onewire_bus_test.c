/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "esp_log.h"
#include "esp_check.h"
#include "sdkconfig.h"
#include "onewire_bus.h"
#include "onewire_device.h"

static const char *TAG = "test-app";

#if CONFIG_EXAMPLE_ONEWIRE_ENABLE_INTERNAL_PULLUP
#define EXAMPLE_ONEWIRE_ENABLE_INTERNAL_PULLUP 1
#else
#define EXAMPLE_ONEWIRE_ENABLE_INTERNAL_PULLUP 0
#endif

void app_main(void)
{
    // install new 1-wire bus
    onewire_bus_handle_t bus;
    onewire_bus_config_t bus_config = {
        .bus_gpio_num = CONFIG_EXAMPLE_ONEWIRE_BUS_GPIO,
        .flags = {
            .en_pull_up = EXAMPLE_ONEWIRE_ENABLE_INTERNAL_PULLUP,
        }
    };
#if CONFIG_EXAMPLE_ONEWIRE_BACKEND_RMT
    onewire_bus_rmt_config_t rmt_config = {
        .max_rx_bytes = CONFIG_EXAMPLE_ONEWIRE_RMT_MAX_RX_BYTES, // 1byte ROM command + 8byte ROM number + 1byte device command
    };
    ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &bus));
    ESP_LOGI(TAG, "1-Wire bus installed on GPIO%d by RMT backend", CONFIG_EXAMPLE_ONEWIRE_BUS_GPIO);
#elif CONFIG_EXAMPLE_ONEWIRE_BACKEND_UART
    onewire_bus_uart_config_t uart_config = {
        .uart_port_num = CONFIG_EXAMPLE_ONEWIRE_UART_PORT_NUM,
    };
    ESP_ERROR_CHECK(onewire_new_bus_uart(&bus_config, &uart_config, &bus));
    ESP_LOGI(TAG, "1-Wire bus installed on GPIO%d by UART backend (UART%d)",
             CONFIG_EXAMPLE_ONEWIRE_BUS_GPIO, CONFIG_EXAMPLE_ONEWIRE_UART_PORT_NUM);
#else
#error "No 1-Wire backend selected in menuconfig"
#endif

    int onewire_device_found = 0;
    onewire_device_iter_handle_t iter = NULL;
    onewire_device_t next_onewire_device;
    esp_err_t search_result = ESP_OK;

    // create 1-wire device iterator, which is used for device search
    ESP_ERROR_CHECK(onewire_new_device_iter(bus, &iter));
    ESP_LOGI(TAG, "Device iterator created, start searching...");
    do {
        search_result = onewire_device_iter_get_next(iter, &next_onewire_device);
        // found a new device
        if (search_result == ESP_OK) {
            ESP_LOGI(TAG, "Found a new device, address: %016llX", next_onewire_device.address);
            onewire_device_found++;
            if (onewire_device_found >= CONFIG_EXAMPLE_ONEWIRE_MAX_DEVICES) {
                ESP_LOGI(TAG, "Max device number reached, stop searching...");
                break;
            }
        }
    } while (search_result != ESP_ERR_NOT_FOUND);
    ESP_ERROR_CHECK(onewire_del_device_iter(iter));
    ESP_LOGI(TAG, "Searching done, %d device(s) found", onewire_device_found);

    // delete the bus
    ESP_LOGI(TAG, "Deleting bus...");
    ESP_ERROR_CHECK(onewire_bus_del(bus));
}
