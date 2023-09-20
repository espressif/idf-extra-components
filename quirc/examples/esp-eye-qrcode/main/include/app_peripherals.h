#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_camera.h"

#define CONFIG_CAMERA_MODULE_ESP_EYE 1
// camera pins
#if CONFIG_CAMERA_MODULE_WROVER_KIT
#define CAMERA_MODULE_NAME "Wrover Kit"
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET -1
#define CAMERA_PIN_XCLK 21
#define CAMERA_PIN_SIOD 26
#define CAMERA_PIN_SIOC 27

#define CAMERA_PIN_D7 35
#define CAMERA_PIN_D6 34
#define CAMERA_PIN_D5 39
#define CAMERA_PIN_D4 36
#define CAMERA_PIN_D3 19
#define CAMERA_PIN_D2 18
#define CAMERA_PIN_D1 5
#define CAMERA_PIN_D0 4
#define CAMERA_PIN_VSYNC 25
#define CAMERA_PIN_HREF 23
#define CAMERA_PIN_PCLK 22

#elif CONFIG_CAMERA_MODULE_ESP_EYE
#define CAMERA_MODULE_NAME "ESP-EYE"
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET -1
#define CAMERA_PIN_XCLK 4
#define CAMERA_PIN_SIOD 18
#define CAMERA_PIN_SIOC 23

#define CAMERA_PIN_D7 36
#define CAMERA_PIN_D6 37
#define CAMERA_PIN_D5 38
#define CAMERA_PIN_D4 39
#define CAMERA_PIN_D3 35
#define CAMERA_PIN_D2 14
#define CAMERA_PIN_D1 13
#define CAMERA_PIN_D0 34
#define CAMERA_PIN_VSYNC 5
#define CAMERA_PIN_HREF 27
#define CAMERA_PIN_PCLK 25

#elif CONFIG_CAMERA_MODULE_ESP_S2_KALUGA
#define CAMERA_MODULE_NAME "ESP-S2-KALUGA"
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET -1
#define CAMERA_PIN_XCLK 1
#define CAMERA_PIN_SIOD 8
#define CAMERA_PIN_SIOC 7

#define CAMERA_PIN_D7 38
#define CAMERA_PIN_D6 21
#define CAMERA_PIN_D5 40
#define CAMERA_PIN_D4 39
#define CAMERA_PIN_D3 42
#define CAMERA_PIN_D2 41
#define CAMERA_PIN_D1 37
#define CAMERA_PIN_D0 36
#define CAMERA_PIN_VSYNC 2
#define CAMERA_PIN_HREF 3
#define CAMERA_PIN_PCLK 33

#elif CONFIG_CAMERA_MODULE_ESP_S3_EYE
#define CAMERA_MODULE_NAME "ESP-S3-EYE"
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET -1

#define CAMERA_PIN_VSYNC 6
#define CAMERA_PIN_HREF 7
#define CAMERA_PIN_PCLK 13
#define CAMERA_PIN_XCLK 15

#define CAMERA_PIN_SIOD 4
#define CAMERA_PIN_SIOC 5

#define CAMERA_PIN_D0 11
#define CAMERA_PIN_D1 9
#define CAMERA_PIN_D2 8
#define CAMERA_PIN_D3 10
#define CAMERA_PIN_D4 12
#define CAMERA_PIN_D5 18
#define CAMERA_PIN_D6 17
#define CAMERA_PIN_D7 16

#elif CONFIG_CAMERA_MODULE_ESP32_CAM_BOARD
#define CAMERA_MODULE_NAME "ESP-DEVCAM"
#define CAMERA_PIN_PWDN 32
#define CAMERA_PIN_RESET 33

#define CAMERA_PIN_XCLK 4
#define CAMERA_PIN_SIOD 18
#define CAMERA_PIN_SIOC 23

#define CAMERA_PIN_D7 36
#define CAMERA_PIN_D6 19
#define CAMERA_PIN_D5 21
#define CAMERA_PIN_D4 39
#define CAMERA_PIN_D3 35
#define CAMERA_PIN_D2 14
#define CAMERA_PIN_D1 13
#define CAMERA_PIN_D0 34
#define CAMERA_PIN_VSYNC 5
#define CAMERA_PIN_HREF 27
#define CAMERA_PIN_PCLK 25

#elif CONFIG_CAMERA_MODULE_M5STACK_PSRAM
#define CAMERA_MODULE_NAME "M5STACK-PSRAM"
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET 15

#define CAMERA_PIN_XCLK 27
#define CAMERA_PIN_SIOD 25
#define CAMERA_PIN_SIOC 23

#define CAMERA_PIN_D7 19
#define CAMERA_PIN_D6 36
#define CAMERA_PIN_D5 18
#define CAMERA_PIN_D4 39
#define CAMERA_PIN_D3 5
#define CAMERA_PIN_D2 34
#define CAMERA_PIN_D1 35
#define CAMERA_PIN_D0 32
#define CAMERA_PIN_VSYNC 22
#define CAMERA_PIN_HREF 26
#define CAMERA_PIN_PCLK 21

#elif CONFIG_CAMERA_MODULE_M5STACK_WIDE
#define CAMERA_MODULE_NAME "M5STACK-WIDE"
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET 15
#define CAMERA_PIN_XCLK 27
#define CAMERA_PIN_SIOD 22
#define CAMERA_PIN_SIOC 23

#define CAMERA_PIN_D7 19
#define CAMERA_PIN_D6 36
#define CAMERA_PIN_D5 18
#define CAMERA_PIN_D4 39
#define CAMERA_PIN_D3 5
#define CAMERA_PIN_D2 34
#define CAMERA_PIN_D1 35
#define CAMERA_PIN_D0 32
#define CAMERA_PIN_VSYNC 25
#define CAMERA_PIN_HREF 26
#define CAMERA_PIN_PCLK 21

#elif CONFIG_CAMERA_MODULE_AI_THINKER
#define CAMERA_MODULE_NAME "AI-THINKER"
#define CAMERA_PIN_PWDN 32
#define CAMERA_PIN_RESET -1
#define CAMERA_PIN_XCLK 0
#define CAMERA_PIN_SIOD 26
#define CAMERA_PIN_SIOC 27

#define CAMERA_PIN_D7 35
#define CAMERA_PIN_D6 34
#define CAMERA_PIN_D5 39
#define CAMERA_PIN_D4 36
#define CAMERA_PIN_D3 21
#define CAMERA_PIN_D2 19
#define CAMERA_PIN_D1 18
#define CAMERA_PIN_D0 5
#define CAMERA_PIN_VSYNC 25
#define CAMERA_PIN_HREF 23
#define CAMERA_PIN_PCLK 22

#elif CONFIG_CAMERA_MODULE_CUSTOM
#define CAMERA_MODULE_NAME "CUSTOM"
#define CAMERA_PIN_PWDN CONFIG_CAMERA_PIN_PWDN
#define CAMERA_PIN_RESET CONFIG_CAMERA_PIN_RESET
#define CAMERA_PIN_XCLK CONFIG_CAMERA_PIN_XCLK
#define CAMERA_PIN_SIOD CONFIG_CAMERA_PIN_SIOD
#define CAMERA_PIN_SIOC CONFIG_CAMERA_PIN_SIOC

#define CAMERA_PIN_D7 CONFIG_CAMERA_PIN_Y9
#define CAMERA_PIN_D6 CONFIG_CAMERA_PIN_Y8
#define CAMERA_PIN_D5 CONFIG_CAMERA_PIN_Y7
#define CAMERA_PIN_D4 CONFIG_CAMERA_PIN_Y6
#define CAMERA_PIN_D3 CONFIG_CAMERA_PIN_Y5
#define CAMERA_PIN_D2 CONFIG_CAMERA_PIN_Y4
#define CAMERA_PIN_D1 CONFIG_CAMERA_PIN_Y3
#define CAMERA_PIN_D0 CONFIG_CAMERA_PIN_Y2
#define CAMERA_PIN_VSYNC CONFIG_CAMERA_PIN_VSYNC
#define CAMERA_PIN_HREF CONFIG_CAMERA_PIN_HREF
#define CAMERA_PIN_PCLK CONFIG_CAMERA_PIN_PCLK
#endif

#define XCLK_FREQ_HZ 20000000
#define CAMERA_PIXFORMAT PIXFORMAT_GRAYSCALE
#define CAMERA_FRAME_SIZE FRAMESIZE_240X240
#define CAMERA_FB_COUNT 2



// lcd pins and setting
#if CONFIG_CAMERA_MODULE_ESP_S3_EYE
#define LCD_CONTROLLER SCREEN_CONTROLLER_ST7789

#define LCD_MOSI 47
#define LCD_MISO -1
#define LCD_SCLK 21
#define LCD_CS 44
#define LCD_DC 43
#define LCD_RST -1
#define LCD_BCKL 48

#define LCD_WIDTH       240
#define LCD_HEIGHT      240
#define LCD_ROTATE      0
#elif CONFIG_CAMERA_MODULE_ESP_S2_KALUGA
#define LCD_CONTROLLER SCREEN_CONTROLLER_ST7789

#define LCD_MOSI 9
#define LCD_MISO -1
#define LCD_SCLK 15
#define LCD_CS 11
#define LCD_DC 13
#define LCD_RST 16
#define LCD_BCKL 6

// LCD display width and height
#define LCD_WIDTH       240
#define LCD_HEIGHT      320
#define LCD_ROTATE      SCR_SWAP_XY|SCR_MIRROR_X
#endif




#ifdef __cplusplus
extern "C"
{
#endif

esp_err_t app_camera_init(void);
#ifdef __cplusplus
}
#endif