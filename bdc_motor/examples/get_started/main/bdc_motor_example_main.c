/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bdc_motor.h"

static const char *TAG = "example";

// MCPWM timer resolution and PWM frequency for the BDC motor
#define BDC_MCPWM_TIMER_RESOLUTION_HZ 10000000 // 10 MHz, 1 tick = 0.1 us
#define BDC_MCPWM_FREQ_HZ             25000    // 25 kHz PWM, above the audible range
#define BDC_MCPWM_DUTY_TICK_MAX       (BDC_MCPWM_TIMER_RESOLUTION_HZ / BDC_MCPWM_FREQ_HZ)

// Connect these GPIOs to the IN1/IN2 (or PWMA/PWMB) pins of an H-bridge
#define BDC_MCPWM_GPIO_A              4
#define BDC_MCPWM_GPIO_B              5

#define BDC_ACTION_DURATION_MS        500

void app_main(void)
{
    ESP_LOGI(TAG, "Create DC motor");
    bdc_motor_config_t motor_config = {
        .pwm_freq_hz = BDC_MCPWM_FREQ_HZ,
        .pwma_gpio_num = BDC_MCPWM_GPIO_A,
        .pwmb_gpio_num = BDC_MCPWM_GPIO_B,
    };
    bdc_motor_mcpwm_config_t mcpwm_config = {
        .group_id = 0,
        .resolution_hz = BDC_MCPWM_TIMER_RESOLUTION_HZ,
    };
    bdc_motor_handle_t motor = NULL;
    ESP_ERROR_CHECK(bdc_motor_new_mcpwm_device(&motor_config, &mcpwm_config, &motor));

    ESP_LOGI(TAG, "Enable motor");
    ESP_ERROR_CHECK(bdc_motor_enable(motor));

    ESP_LOGI(TAG, "Set speed to 50%%");
    ESP_ERROR_CHECK(bdc_motor_set_speed(motor, BDC_MCPWM_DUTY_TICK_MAX / 2));

    while (1) {
        ESP_LOGI(TAG, "Motor forward");
        ESP_ERROR_CHECK(bdc_motor_forward(motor));
        vTaskDelay(pdMS_TO_TICKS(BDC_ACTION_DURATION_MS));

        ESP_LOGI(TAG, "Motor reverse");
        ESP_ERROR_CHECK(bdc_motor_reverse(motor));
        vTaskDelay(pdMS_TO_TICKS(BDC_ACTION_DURATION_MS));

        // Fast decay: both sides of the H-bridge are left floating
        ESP_LOGI(TAG, "Motor coast");
        ESP_ERROR_CHECK(bdc_motor_coast(motor));
        vTaskDelay(pdMS_TO_TICKS(BDC_ACTION_DURATION_MS));

        // Slow decay: both sides of the H-bridge are driven to the same level
        ESP_LOGI(TAG, "Motor brake");
        ESP_ERROR_CHECK(bdc_motor_brake(motor));
        vTaskDelay(pdMS_TO_TICKS(BDC_ACTION_DURATION_MS));
    }
}
