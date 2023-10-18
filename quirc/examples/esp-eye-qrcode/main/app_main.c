#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "esp_camera.h"
#include "freertos/task.h"
//#include "app_peripherals.h"
#include "quirc.h"

static const char *TAG = "APP_CODE_SCANNER";

static void decode_task(void *args)
{

    bsp_i2c_init();
    bsp_display_start();
    bsp_display_backlight_on(); // Set display brightness to 100%

    bsp_leds_init();
    bsp_led_set(BSP_LED_GREEN, false);

    /* Camera init start*/
    camera_config_t camera_config = BSP_CAMERA_DEFAULT_CONFIG;
    //camera_config.pixel_format = PIXFORMAT_GRAYSCALE;
    //camera_config.xclk_freq_hz = 20000000;
    if (esp_camera_init(&camera_config) != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        exit(1);
    }
    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1);


    ESP_LOGI(TAG, "Camera Init done");
    /* Camera init end*/


    bsp_display_lock(0);
    lv_obj_t *camera_canvas = lv_canvas_create(lv_scr_act());
    assert(camera_canvas);
    lv_obj_center(camera_canvas);
    bsp_display_unlock();

    camera_fb_t *fb = NULL;

    /* Initializing the quirc handle */
    struct quirc *q = quirc_new();
    if (!q) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        exit(1);
    }


    /* Get image size through fb parameters */
    fb = esp_camera_fb_get();
    if (fb == NULL) {
        ESP_LOGE(TAG, "Camera get failed");
        exit(1);
    }
    bsp_display_lock(1000);
    lv_canvas_set_buffer(camera_canvas, fb->buf, fb->width, fb->height, LV_IMG_CF_TRUE_COLOR);
    bsp_display_unlock();
    uint16_t p_width = fb->width;
    uint16_t p_height = fb->height;

    printf("Detected width:%d \t height:%d \n\n", p_width, p_height);

    if (quirc_resize(q, p_width, p_height) < 0) {
        ESP_LOGE(TAG, "Failed to allocate video  memory\n");
        exit(1);
    }
    bsp_display_lock(1000);
    lv_canvas_set_buffer(camera_canvas, fb->buf, fb->width, fb->height, LV_IMG_CF_TRUE_COLOR);
    bsp_display_unlock();
    esp_camera_fb_return(fb);


    struct quirc_code code;
    struct quirc_data data;
    quirc_decode_error_t err;
    uint16_t num_codes;

    while (1) {
        fb = esp_camera_fb_get();
        if (fb == NULL) {
            ESP_LOGE(TAG, "Camera get failed");
            exit(1);
        }

        // Decode Progress

        memcpy(quirc_begin(q, NULL, NULL), fb->buf, fb->len);
        quirc_end(q);

        num_codes = quirc_count(q);
        for (uint16_t i = 0; i < num_codes; i++) {

            quirc_extract(q, i, &code);
            /* Decoding stage */
            err = quirc_decode(&code, &data);
            if (err) {
                printf("%d/%d] DECODE FAILED: %s\n", i + 1, num_codes, quirc_strerror(err));
            } else {
                printf("%d/%d] DATA: %s\n", i + 1, num_codes, data.payload);
                bsp_led_set(BSP_LED_GREEN, true);
            }
        }

        esp_camera_fb_return(fb);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        bsp_led_set(BSP_LED_GREEN, false);
    }

}


void app_main(void)
{

    xTaskCreatePinnedToCore(decode_task, TAG, 40 * 1024, NULL, 6, NULL, 0);
}