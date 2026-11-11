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

#if CONFIG_EXAMPLE_ONEWIRE_BACKEND_UART
#define EXAMPLE_ONEWIRE_UART_PORT_NUM CONFIG_EXAMPLE_ONEWIRE_UART_PORT_NUM
#endif

#define EXAMPLE_ONEWIRE_BUS_GPIO     CONFIG_EXAMPLE_ONEWIRE_BUS_GPIO
#define EXAMPLE_ONEWIRE_MAX_DEVICES  CONFIG_EXAMPLE_ONEWIRE_MAX_DEVICES

// search the bus with the given iterator and delete it, return the number of devices found
static int search_devices(onewire_device_iter_handle_t iter)
{
    int device_found = 0;
    onewire_device_t next_onewire_device;
    esp_err_t search_result;
    while ((search_result = onewire_device_iter_get_next(iter, &next_onewire_device)) == ESP_OK) {
        ESP_LOGI(TAG, "Found a new device, address: %016llX", next_onewire_device.address);
        device_found++;
        if (device_found >= EXAMPLE_ONEWIRE_MAX_DEVICES) {
            ESP_LOGI(TAG, "Max device number reached, stop searching...");
            break;
        }
    }
    ESP_ERROR_CHECK(onewire_del_device_iter(iter));
    // ESP_ERR_NOT_FOUND marks the end of the search, any other error is a bus failure
    if (search_result != ESP_ERR_NOT_FOUND) {
        ESP_ERROR_CHECK(search_result);
    }
    return device_found;
}

void app_main(void)
{
    // install new 1-wire bus
    onewire_bus_handle_t bus;
    onewire_bus_config_t bus_config = {
        .bus_gpio_num = EXAMPLE_ONEWIRE_BUS_GPIO,
        .flags = {
            .en_pull_up = EXAMPLE_ONEWIRE_ENABLE_INTERNAL_PULLUP,
        }
    };
#if CONFIG_EXAMPLE_ONEWIRE_BACKEND_RMT
    onewire_bus_rmt_config_t rmt_config = {
        .max_rx_bytes = 10, // 1byte ROM command + 8byte ROM number + 1byte device command
    };
    ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &bus));
    ESP_LOGI(TAG, "1-Wire bus installed on GPIO%d by RMT backend", EXAMPLE_ONEWIRE_BUS_GPIO);
#elif CONFIG_EXAMPLE_ONEWIRE_BACKEND_UART
    onewire_bus_uart_config_t uart_config = {
        .uart_port_num = EXAMPLE_ONEWIRE_UART_PORT_NUM,
    };
    ESP_ERROR_CHECK(onewire_new_bus_uart(&bus_config, &uart_config, &bus));
    ESP_LOGI(TAG, "1-Wire bus installed on GPIO%d by UART backend (UART%d)",
             EXAMPLE_ONEWIRE_BUS_GPIO, EXAMPLE_ONEWIRE_UART_PORT_NUM);
#else
#error "No 1-Wire backend selected in menuconfig"
#endif

    onewire_device_iter_handle_t iter = NULL;

    // create 1-wire device iterator, which is used for device search
    ESP_ERROR_CHECK(onewire_new_device_iter(bus, &iter));
    ESP_LOGI(TAG, "Device iterator created, start searching...");
    int onewire_device_found = search_devices(iter);
    ESP_LOGI(TAG, "Searching done, %d device(s) found", onewire_device_found);

    // search only for the devices of one family (DS18B20)
    ESP_ERROR_CHECK(onewire_new_device_iter_family(bus, 0x28, &iter));
    ESP_LOGI(TAG, "Family device iterator created, start searching...");
    int onewire_family_device_found = search_devices(iter);
    ESP_LOGI(TAG, "Family searching done, %d device(s) found", onewire_family_device_found);

    // delete the bus
    ESP_LOGI(TAG, "Deleting bus...");
    ESP_ERROR_CHECK(onewire_bus_del(bus));
}
