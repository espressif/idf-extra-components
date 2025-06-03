/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_rom_gpio.h"
#include "led_strip.h"
#include "led_strip_interface.h"

#define LED_STRIP_PARLIO_DEFAULT_RESOLUTION (2.5 * 1000 * 1000) // 2.5MHz resolution
#define LED_STRIP_PARLIO_RESET_TIME 20 // Reset time between two frames, 20 * 400 ns/bit * 8 bit = 64 us

// Each color of 1 bit is represented by 3 bits of PARLIO, low_level:100 ,high_level:110
#define PARLIO_DATA_BITS_PER_COLOR_BIT 3

static const char *TAG = "led_strip_parlio";

typedef struct led_strip_parlio_group_t led_strip_parlio_group_t;
typedef struct led_strip_parlio_obj led_strip_parlio_obj;

struct led_strip_parlio_obj {
    led_strip_t base;
    uint8_t strip_index;
    led_strip_parlio_group_t *parlio_group;
};

struct led_strip_parlio_group_t {
    led_strip_group_t base; // Base object
    uint8_t strip_count;    // Number of the LED strip channels
    uint8_t buffer_bytes_per_color;  // PARLIO buffer bytes occupied by 1 color byte
    uint8_t bytes_per_pixel; // Number of bytes per pixel
    uint32_t strip_len;     // Number of LEDs in one strip
    parlio_tx_unit_handle_t tx_unit; // PARLIO TX unit handle
    led_color_component_format_t component_fmt;  // LED color component format
    led_strip_parlio_obj *parlio_strip[SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH]; // PARLIO LED strip object
    uint8_t pixel_buf[];
};

static void __led_strip_parlio_bit(uint8_t data, uint8_t *buf, uint8_t strip_count, uint8_t strip_index)
{
    // Each color of 1 bit is represented by 3 bits of PARLIO, low_level:100 ,high_level:110
    // So a color byte occupies 24 data bits of PARLIO.
    // And buffer byte is share by all strip
    // So 1 data bit occupies (strip_count / 8) buffer bits

    /*
     * Taking 4 strips as an example, the bit distribution of each color byte is as follows:
     *
     * Original data bits:
     * Strip 0:  G7 G6 G5 G4 G3 G2 G1 G0 B7 B6 B5 B4 B3 B2 B1 B0 R7 R6 R5 R4 R3 R2 R1 R0
     * Strip 1:  G7 G6 G5 G4 G3 G2 G1 G0 B7 B6 B5 B4 B3 B2 B1 B0 R7 R6 R5 R4 R3 R2 R1 R0
     * Strip 2:  G7 G6 G5 G4 G3 G2 G1 G0 B7 B6 B5 B4 B3 B2 B1 B0 R7 R6 R5 R4 R3 R2 R1 R0
     * Strip 3:  G7 G6 G5 G4 G3 G2 G1 G0 B7 B6 B5 B4 B3 B2 B1 B0 R7 R6 R5 R4 R3 R2 R1 R0
     *
     * After PARLIO encoding, the distribution in the buffer is as follows:
     *
     *    buffer:    uint_8   uint_8   uint_8   uint_8   uint_8   uint_8   uint_8   uint_8
     *             +--------+--------+--------+--------+--------+--------+--------+--------+
     *    Strip 0  | G7  G7 | G7  G6 | G6  G6 | ...... | ...... | R1  R1 | R1  R0 | R0  R0 |
     *    Strip 1  | G7  G7 | G7  G6 | G6  G6 | ...... | ...... | R1  R1 | R1  R0 | R0  R0 |
     *    Strip 2  | G7  G7 | G7  G6 | G6  G6 | ...... | ...... | R1  R1 | R1  R0 | R0  R0 |
     *    Strip 3  | G7  G7 | G7  G6 | G6  G6 | ...... | ...... | R1  R1 | R1  R0 | R0  R0 |
     *             +--------+--------+--------+--------+--------+--------+--------+--------+
     */

    uint8_t buffer_index = 0;
    uint8_t logic_dilimiter = strip_index;
    // Start with the MSB (Bit 7)
    for (int i = 7; i >= 0; i--) {
        while (logic_dilimiter >= 8) {
            logic_dilimiter -= 8;
            buffer_index += 1;
        }
        *(buf + buffer_index) |= BIT(logic_dilimiter);
        logic_dilimiter += strip_count;
        while (logic_dilimiter >= 8) {
            logic_dilimiter -= 8;
            buffer_index += 1;
        }
        if (data & BIT(i)) {
            *(buf + buffer_index) |= BIT(logic_dilimiter);
        } else {
            *(buf + buffer_index) &= ~BIT(logic_dilimiter);
        }
        // the last color bit is always 0, skip
        logic_dilimiter += 2 * strip_count;
    }
}

static esp_err_t led_strip_parlio_set_pixel(led_strip_t *strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue)
{
    led_strip_parlio_obj *parlio_strip = __containerof(strip, led_strip_parlio_obj, base);
    led_strip_parlio_group_t *parlio_group = parlio_strip->parlio_group;
    ESP_RETURN_ON_FALSE(index < parlio_group->strip_len, ESP_ERR_INVALID_ARG, TAG, "index out of maximum number of LEDs");
    // 1 pixel needs 3 color bytes, 1 color byte needs (PARLIO_DATA_BITS_PER_COLOR_BIT * strip_count) bytes

    uint8_t strip_count = parlio_group->strip_count;
    uint8_t buffer_bytes_per_color = parlio_group->buffer_bytes_per_color;
    uint32_t start = index * parlio_group->bytes_per_pixel * buffer_bytes_per_color;
    uint8_t *pixel_buf = parlio_group->pixel_buf;
    led_color_component_format_t component_fmt = parlio_group->component_fmt;

    __led_strip_parlio_bit(red, &pixel_buf[start + buffer_bytes_per_color * component_fmt.format.r_pos], strip_count, parlio_strip->strip_index);
    __led_strip_parlio_bit(green, &pixel_buf[start + buffer_bytes_per_color * component_fmt.format.g_pos], strip_count, parlio_strip->strip_index);
    __led_strip_parlio_bit(blue, &pixel_buf[start + buffer_bytes_per_color * component_fmt.format.b_pos], strip_count, parlio_strip->strip_index);
    if (component_fmt.format.num_components > 3) {
        __led_strip_parlio_bit(0, &pixel_buf[start + buffer_bytes_per_color * component_fmt.format.w_pos], strip_count, parlio_strip->strip_index);
    }

    return ESP_OK;
}

static esp_err_t led_strip_parlio_set_pixel_rgbw(led_strip_t *strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint32_t white)
{
    led_strip_parlio_obj *parlio_strip = __containerof(strip, led_strip_parlio_obj, base);
    led_strip_parlio_group_t *parlio_group = parlio_strip->parlio_group;
    led_color_component_format_t component_fmt = parlio_group->component_fmt;
    ESP_RETURN_ON_FALSE(index < parlio_group->strip_len, ESP_ERR_INVALID_ARG, TAG, "index out of maximum number of LEDs");
    ESP_RETURN_ON_FALSE(component_fmt.format.num_components == 4, ESP_ERR_INVALID_ARG, TAG, "led doesn't have 4 components");

    uint8_t strip_count = parlio_group->strip_count;
    uint8_t buffer_bytes_per_color = parlio_group->buffer_bytes_per_color;
    uint32_t start = index * parlio_group->bytes_per_pixel * buffer_bytes_per_color;
    uint8_t *pixel_buf = parlio_group->pixel_buf;

    __led_strip_parlio_bit(red, &pixel_buf[start + buffer_bytes_per_color * component_fmt.format.r_pos], strip_count, parlio_strip->strip_index);
    __led_strip_parlio_bit(green, &pixel_buf[start + buffer_bytes_per_color * component_fmt.format.g_pos], strip_count, parlio_strip->strip_index);
    __led_strip_parlio_bit(blue, &pixel_buf[start + buffer_bytes_per_color * component_fmt.format.b_pos], strip_count, parlio_strip->strip_index);
    __led_strip_parlio_bit(white, &pixel_buf[start + buffer_bytes_per_color * component_fmt.format.w_pos], strip_count, parlio_strip->strip_index);

    return ESP_OK;
}

static esp_err_t led_strip_parlio_refresh_async(led_strip_t *strip)
{
    led_strip_parlio_obj *parlio_strip = __containerof(strip, led_strip_parlio_obj, base);
    led_strip_parlio_group_t *parlio_group = parlio_strip->parlio_group;
    parlio_transmit_config_t transmit_config = {
        .idle_value = 0x00,
        .flags.queue_nonblocking = true,
    };
    parlio_tx_unit_handle_t tx_unit = parlio_group->tx_unit;
    uint8_t *tx_buffer = parlio_group->pixel_buf;
    size_t tx_length = parlio_group->strip_len * parlio_group->bytes_per_pixel * parlio_group->buffer_bytes_per_color * 8;
    size_t reset_length = parlio_group->strip_count * LED_STRIP_PARLIO_RESET_TIME * 8;
    ESP_RETURN_ON_ERROR(parlio_tx_unit_enable(parlio_group->tx_unit), TAG, "enable parlio unit failed");
    ESP_RETURN_ON_ERROR(parlio_tx_unit_transmit(tx_unit, tx_buffer, tx_length + reset_length, &transmit_config), TAG, "transmit pixels by PARLIO failed");
    return ESP_OK;
}

static esp_err_t led_strip_parlio_refresh_wait_async_done(led_strip_t *strip)
{
    led_strip_parlio_obj *parlio_strip = __containerof(strip, led_strip_parlio_obj, base);
    led_strip_parlio_group_t *parlio_group = parlio_strip->parlio_group;
    ESP_RETURN_ON_ERROR(parlio_tx_unit_wait_all_done(parlio_group->tx_unit, -1), TAG, "wait for done failed");
    ESP_RETURN_ON_ERROR(parlio_tx_unit_disable(parlio_group->tx_unit), TAG, "disable parlio unit failed");
    return ESP_OK;
}

static esp_err_t led_strip_parlio_refresh(led_strip_t *strip)
{
    ESP_RETURN_ON_ERROR(led_strip_parlio_refresh_async(strip), TAG, "refresh failed");
    ESP_RETURN_ON_ERROR(led_strip_parlio_refresh_wait_async_done(strip), TAG, "wait for done failed");
    return ESP_OK;
}

static esp_err_t led_strip_parlio_clear(led_strip_t *strip)
{
    led_strip_parlio_obj *parlio_strip = __containerof(strip, led_strip_parlio_obj, base);
    led_strip_parlio_group_t *parlio_group = parlio_strip->parlio_group;
    uint8_t buffer_bytes_per_color = parlio_group->buffer_bytes_per_color;

    uint8_t *buf = parlio_group->pixel_buf;
    for (int index = 0; index < parlio_group->strip_len * parlio_group->bytes_per_pixel; index++) {
        __led_strip_parlio_bit(0, buf, parlio_group->strip_count, parlio_strip->strip_index);
        buf += buffer_bytes_per_color;
    }

    return led_strip_parlio_refresh(strip);
}

static esp_err_t led_strip_parlio_del(led_strip_t *strip)
{
    ESP_RETURN_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, TAG, "please call ""led_strip_group_del"" to delete the group");
    return ESP_OK;
}

static esp_err_t led_strip_parlio_group_get_strip_handle(led_strip_group_t *strip_group, uint8_t index, led_strip_handle_t *ret_strip)
{
    led_strip_parlio_group_t *parlio_group = __containerof(strip_group, led_strip_parlio_group_t, base);
    ESP_RETURN_ON_FALSE(index <= parlio_group->strip_count, ESP_ERR_INVALID_ARG, TAG, "invalid index");
    *ret_strip = &parlio_group->parlio_strip[index]->base;
    return ESP_OK;
}

static esp_err_t led_strip_parlio_group_del(led_strip_group_t *strip_group)
{
    led_strip_parlio_group_t *parlio_group = __containerof(strip_group, led_strip_parlio_group_t, base);

    if (parlio_group->tx_unit) {
        ESP_RETURN_ON_ERROR(parlio_del_tx_unit(parlio_group->tx_unit), TAG, "delete parlio_tx failed");
    }
    for (int i = 0; i < parlio_group->strip_count; i++) {
        if (parlio_group->parlio_strip[i]) {
            free(parlio_group->parlio_strip[i]);
        }
    }
    free(parlio_group);

    return ESP_OK;
}

esp_err_t led_strip_new_parlio_group(const led_strip_config_t *led_config, const led_strip_parlio_config_t *parlio_config, led_strip_group_handle_t *ret_group)
{
    led_strip_parlio_group_t *parlio_group = NULL;
    led_strip_parlio_obj *parlio_strip = NULL;
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(led_config && parlio_config && ret_group, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    // strip_count must be power of 2 and less than or equal to SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH
    ESP_RETURN_ON_FALSE(parlio_config->strip_count && (parlio_config->strip_count <= SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH) && ((parlio_config->strip_count & (parlio_config->strip_count - 1)) == 0),
                        ESP_ERR_INVALID_ARG, TAG, "invalid strip count");

    led_color_component_format_t component_fmt = led_config->color_component_format;
    // If R/G/B order is not specified, set default GRB order as fallback
    if (component_fmt.format_id == 0) {
        component_fmt = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    }
    // check the validation of the color component format
    uint8_t mask = 0;
    if (component_fmt.format.num_components == 3) {
        mask = BIT(component_fmt.format.r_pos) | BIT(component_fmt.format.g_pos) | BIT(component_fmt.format.b_pos);
        // Check for invalid values
        ESP_RETURN_ON_FALSE(mask == 0x07, ESP_ERR_INVALID_ARG, TAG, "invalid order argument");
    } else if (component_fmt.format.num_components == 4) {
        mask = BIT(component_fmt.format.r_pos) | BIT(component_fmt.format.g_pos) | BIT(component_fmt.format.b_pos) | BIT(component_fmt.format.w_pos);
        // Check for invalid values
        ESP_RETURN_ON_FALSE(mask == 0x0F, ESP_ERR_INVALID_ARG, TAG, "invalid order argument");
    } else {
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_ARG, TAG, "invalid number of color components: %d", component_fmt.format.num_components);
    }
    // TODO: we assume each color component is 8 bits, may need to support other configurations in the future, e.g. 10bits per color component?
    uint8_t bytes_per_pixel = component_fmt.format.num_components;
    uint32_t mem_caps = MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;

    // buffer is share by all strip
    uint32_t buffer_bytes_per_color = parlio_config->strip_count * PARLIO_DATA_BITS_PER_COLOR_BIT;
    parlio_group = heap_caps_calloc(1, sizeof(led_strip_parlio_group_t) + led_config->max_leds * bytes_per_pixel * buffer_bytes_per_color + parlio_config->strip_count * LED_STRIP_PARLIO_RESET_TIME, mem_caps);
    parlio_strip = heap_caps_calloc(parlio_config->strip_count, sizeof(led_strip_parlio_obj), mem_caps);
    ESP_GOTO_ON_FALSE(parlio_group && parlio_strip, ESP_ERR_NO_MEM, err, TAG, "no mem for parlio strip");

    // for backward compatibility, if the user does not set the clk_src, use the default value
    parlio_clock_source_t clk_src = PARLIO_CLK_SRC_DEFAULT;
    if (parlio_config->clk_src) {
        clk_src = parlio_config->clk_src;
    }

    parlio_tx_unit_config_t parlio_tx_unit_config = {
        .clk_src = clk_src,
        .data_width = parlio_config->strip_count,
        .clk_in_gpio_num = -1,
        .clk_out_gpio_num = -1,
        .output_clk_freq_hz = LED_STRIP_PARLIO_DEFAULT_RESOLUTION,
        .trans_queue_depth = 1, // Only have one pixel_buf, so only one transaction is needed
        .max_transfer_size = led_config->max_leds * bytes_per_pixel * buffer_bytes_per_color + LED_STRIP_PARLIO_RESET_TIME * parlio_config->strip_count,
        .valid_gpio_num = -1,
    };
    memcpy(parlio_tx_unit_config.data_gpio_nums, parlio_config->strip_gpio_num, parlio_config->strip_count * sizeof(parlio_config->strip_gpio_num[0]));

    ESP_GOTO_ON_ERROR(parlio_new_tx_unit(&parlio_tx_unit_config, &parlio_group->tx_unit), err, TAG, "init parlio unit failed");

    //ensure the reset time is enough
    esp_rom_delay_us(10);

    parlio_group->buffer_bytes_per_color = buffer_bytes_per_color;
    parlio_group->component_fmt = component_fmt;
    parlio_group->bytes_per_pixel = bytes_per_pixel;
    parlio_group->strip_len = led_config->max_leds;
    parlio_group->strip_count = parlio_config->strip_count;
    parlio_group->base.get_strip_handle = led_strip_parlio_group_get_strip_handle;
    parlio_group->base.del = led_strip_parlio_group_del;

    for (int i = 0; i < parlio_group->strip_count; i++) {
        parlio_strip[i].base.set_pixel = led_strip_parlio_set_pixel;
        parlio_strip[i].base.set_pixel_rgbw = led_strip_parlio_set_pixel_rgbw;
        parlio_strip[i].base.refresh = led_strip_parlio_refresh;
        parlio_strip[i].base.refresh_async = led_strip_parlio_refresh_async;
        parlio_strip[i].base.refresh_wait_async_done = led_strip_parlio_refresh_wait_async_done;
        parlio_strip[i].base.clear = led_strip_parlio_clear;
        parlio_strip[i].base.del = led_strip_parlio_del;
        parlio_strip[i].strip_index = i;
        parlio_strip[i].parlio_group = parlio_group;
        parlio_group->parlio_strip[i] = &parlio_strip[i];
    }
    *ret_group = &parlio_group->base;
    return ESP_OK;
err:
    if (parlio_group) {
        if (parlio_group->tx_unit) {
            parlio_del_tx_unit(parlio_group->tx_unit);
        }
        free(parlio_group);
    }
    if (parlio_strip) {
        free(parlio_strip);
    }
    return ret;
}
