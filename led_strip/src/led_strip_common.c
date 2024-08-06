#include <stdbool.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_private/led_strip_common.h"

static const char *TAG = "led_strip_common";
esp_err_t led_strip_config_pixel_order(uint8_t *led_pixel_offset, led_pixel_format_t led_pixel_format)
{
    ESP_RETURN_ON_FALSE(led_pixel_offset, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    switch (led_pixel_format) {
    case LED_PIXEL_FORMAT_GRB:
        led_pixel_offset[0] = 1;
        led_pixel_offset[1] = 0;
        led_pixel_offset[2] = 2;
        break;
    case LED_PIXEL_FORMAT_RGB:
        led_pixel_offset[0] = 0;
        led_pixel_offset[1] = 1;
        led_pixel_offset[2] = 2;
        break;
    case LED_PIXEL_FORMAT_GRBW:
        led_pixel_offset[0] = 0;
        led_pixel_offset[1] = 2;
        led_pixel_offset[2] = 1;
        led_pixel_offset[3] = 3;
        break;
    case LED_PIXEL_FORMAT_RGBW:
        led_pixel_offset[0] = 0;
        led_pixel_offset[1] = 1;
        led_pixel_offset[2] = 2;
        led_pixel_offset[3] = 3;
        break;
    default:
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_ARG, TAG, "invalid pixel format");
    }
    return ESP_OK;
}
