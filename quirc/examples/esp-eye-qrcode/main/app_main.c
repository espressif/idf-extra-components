#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_peripherals.h"
#include "quirc.h"

static const char *TAG = "APP_CODE_SCANNER";

static void decode_task(void *args)
{
    if (app_camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init camera");
        exit(1);
    }

    camera_fb_t *fb = NULL;

    // Initializing the quirc handle
    struct quirc *q = quirc_new();
    if (!q) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        exit(1);
    }


    // Get image size through fb parameters
    fb = esp_camera_fb_get();
    if (fb == NULL) {
        ESP_LOGE(TAG, "Camera get failed");
        exit(1);
    }

    uint16_t p_width = fb->width;
    uint16_t p_height = fb->height;


    if (quirc_resize(q, p_width, p_height) < 0) {
        ESP_LOGE(TAG, "Failed to allocate video  memory\n");
        exit(1);
    }

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
            }
        }

        esp_camera_fb_return(fb);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    quirc_destroy(q);
}


void app_main(void)
{
    xTaskCreatePinnedToCore(decode_task, TAG, 40 * 1024, NULL, 6, NULL, 0);
}