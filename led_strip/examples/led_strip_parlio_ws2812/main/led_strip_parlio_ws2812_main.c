/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"

// GPIO assignment
#define LED_STRIP0_GPIO_PIN  0
#define LED_STRIP1_GPIO_PIN  1
#define LED_STRIP2_GPIO_PIN  2
#define LED_STRIP3_GPIO_PIN  3

// Numbers of the LED in the strip
#define LED_STRIP_LED_COUNT 8
// Numbers of the strip
#define LED_STRIP_COUNT 4

static const char *TAG = "example";

led_strip_handle_t *configure_led(void)
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .max_leds = LED_STRIP_LED_COUNT,      // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812,        // LED strip model
        // set the color order of the strip: GRB
        .color_component_format = {
            .format = {
                .r_pos = 1, // red is the second byte in the color data
                .g_pos = 0, // green is the first byte in the color data
                .b_pos = 2, // blue is the third byte in the color data
                .num_components = 3, // total 3 color components
            },
        },
        .flags = {
            .invert_out = false, // don't invert the output signal
        }
    };

    // LED strip backend configuration: PARLIO
    led_strip_parlio_config_t parlio_config = {
        .clk_src = PARLIO_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
        .strip_count = LED_STRIP_COUNT,
        .strip_gpio_num = {
            LED_STRIP0_GPIO_PIN,
            LED_STRIP1_GPIO_PIN,
            LED_STRIP2_GPIO_PIN,
            LED_STRIP3_GPIO_PIN,
        },
    };

    // LED Strip group handle
    led_strip_group_handle_t parlio_group;
    ESP_ERROR_CHECK(led_strip_new_parlio_group(&strip_config, &parlio_config, &parlio_group));
    led_strip_handle_t *led_strip = calloc(LED_STRIP_COUNT, sizeof(led_strip_handle_t));
    for (int i = 0; i < LED_STRIP_COUNT; i++) {
        ESP_ERROR_CHECK(led_strip_group_get_strip_handle(parlio_group, i, &led_strip[i]));
    }

    ESP_LOGI(TAG, "Created LED strip object with PARLIO backend");
    return led_strip;
}

void app_main(void)
{
    led_strip_handle_t *led_strip = configure_led();
    bool led_on_off = false;

    ESP_LOGI(TAG, "Start blinking LED strip");
    while (1) {
        if (led_on_off) {
            /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
            for (int i = 0; i < LED_STRIP_COUNT; i++) {
                for (int j = 0; j < LED_STRIP_LED_COUNT; j++) {
                    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip[i], j, 5, 5, 5));
                }
            }

            /* Refresh the strip to send data */
            for (int i = 0; i < LED_STRIP_COUNT; i++) {
                ESP_ERROR_CHECK(led_strip_refresh(led_strip[i]));
            }
            ESP_LOGI(TAG, "LED ON!");
        } else {
            /* Set all LED off to clear all pixels */
            for (int i = 0; i < LED_STRIP_COUNT; i++) {
                ESP_ERROR_CHECK(led_strip_clear(led_strip[i]));
            }
            ESP_LOGI(TAG, "LED OFF!");
        }

        led_on_off = !led_on_off;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
