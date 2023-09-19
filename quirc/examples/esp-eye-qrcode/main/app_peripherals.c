#include "app_peripherals.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "app_peripherals";

esp_err_t app_camera_init()
{
    ESP_LOGI(TAG, "Camera module is %s", CAMERA_MODULE_NAME);

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAMERA_PIN_D0;
    config.pin_d1 = CAMERA_PIN_D1;
    config.pin_d2 = CAMERA_PIN_D2;
    config.pin_d3 = CAMERA_PIN_D3;
    config.pin_d4 = CAMERA_PIN_D4;
    config.pin_d5 = CAMERA_PIN_D5;
    config.pin_d6 = CAMERA_PIN_D6;
    config.pin_d7 = CAMERA_PIN_D7;
    config.pin_xclk = CAMERA_PIN_XCLK;
    config.pin_pclk = CAMERA_PIN_PCLK;
    config.pin_vsync = CAMERA_PIN_VSYNC;
    config.pin_href = CAMERA_PIN_HREF;
    config.pin_sccb_sda = CAMERA_PIN_SIOD;
    config.pin_sccb_scl = CAMERA_PIN_SIOC;
    config.pin_pwdn = CAMERA_PIN_PWDN;
    config.pin_reset = CAMERA_PIN_RESET;
    config.xclk_freq_hz = XCLK_FREQ_HZ;
    config.pixel_format = CAMERA_PIXFORMAT;
    config.frame_size = CAMERA_FRAME_SIZE;
    config.jpeg_quality = 5;
    config.fb_count = CAMERA_FB_COUNT;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return ESP_FAIL;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID || s->id.PID == OV2640_PID)
        s->set_vflip(s, 1); //flip it back
    else if (s->id.PID == GC0308_PID){
        s->set_hmirror(s, 0);
    }
    else if (s->id.PID == GC032A_PID){
        s->set_vflip(s, 1);
    }

    if (s->id.PID == OV3660_PID)
    {
        s->set_brightness(s, 2);
        s->set_contrast(s, 3);
    }

    return ESP_OK;
}


