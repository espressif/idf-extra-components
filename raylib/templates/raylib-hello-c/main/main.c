/**
 * @file main.c
 * @brief Raylib example
 *
 * Architecture:
 * - BSP handles display hardware (for boards with BSP components)
 * - Direct esp_lcd init for boards without BSP (e.g., esp32_s3_box)
 * - rcore callback interface connects raylib to display
 * - No separate port layer needed
 *
 * Board configurations sourced from esp-bsp JSON files:
 * /path/to/esp-bsp/bsp/<board_name>/<board_name>.json
 */

//IF option("esp32_s3_box")
#include "esp_log.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_st7789.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "raylib.h"
//ELIF option("m5stack_atom_s3r")
#include "esp_log.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_st7789.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "raylib.h"
//ELSE
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "raylib.h"
//ENDIF

//IF option("esp32_s3_box_3")
static const char *TAG = "ESP32S3_BOX_3";
//ELIF option("esp32_s3_lcd_ev_board")
static const char *TAG = "ESP32_S3_LCD_EV";
//ELIF option("esp32s3_korvo_2")
static const char *TAG = "ESP32S3_KORVO_2";
//ELIF option("esp32_s3_box")
static const char *TAG = "ESP32_S3_BOX";
//ELIF option("esp_vocat")
static const char *TAG = "ESP_VOCAT";
//ELIF option("esp32_s3_eye")
static const char *TAG = "ESP32_S3_EYE";
//ELIF option("m5stack_core_2")
static const char *TAG = "M5STACK_CORE2";
//ELIF option("m5stack_atom_s3")
static const char *TAG = "M5STACK_ATOMS3";
//ELIF option("m5stack_atom_s3r")
static const char *TAG = "M5STACK_ATOMS3R";
//ELIF option("m5stack_core_s3")
static const char *TAG = "M5STACK_CORES3";
//ENDIF

// Display handles from BSP
static esp_lcd_panel_handle_t g_panel = NULL;
static esp_lcd_panel_io_handle_t g_io = NULL;

// Chunk size: 48 lines (matches BSP max_transfer_sz)
#define CHUNK_LINES 48

/**
 * @brief Display flush callback for rcore - chunks framebuffer
 */
static void display_flush(const uint16_t *buf, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    if (!g_panel || !buf) {
        return;
    }

    // Flush in chunks (48 lines max) to match BSP max_transfer_sz
    for (uint16_t row = 0; row < h; row += CHUNK_LINES) {
        uint16_t chunk_height = (row + CHUNK_LINES > h) ? (h - row) : CHUNK_LINES;
        const uint16_t *chunk_pixels = buf + (row * w);

        // Swap bytes for RGB565 (little-endian ESP32 to big-endian SPI LCD)
        // Allocate temporary buffer for swapped bytes
        uint16_t *swapped_buf = heap_caps_malloc(chunk_height * w * sizeof(uint16_t), MALLOC_CAP_DMA);
        if (!swapped_buf) {
            ESP_LOGE(TAG, "Failed to allocate swap buffer");
            return;
        }
        for (int i = 0; i < chunk_height * w; i++) {
            swapped_buf[i] = __builtin_bswap16(chunk_pixels[i]);
        }

        esp_err_t ret = esp_lcd_panel_draw_bitmap(
            g_panel,
            x, y + row, x + w, y + row + chunk_height,
            swapped_buf
        );
        heap_caps_free(swapped_buf);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to draw bitmap chunk at row %d: %s", row, esp_err_to_name(ret));
            return;
        }
    }
}

/**
 * @brief Get display dimensions callback
 * Source: esp-bsp/bsp/<board>/<board>.json -> BSP_LCD_H_RES, BSP_LCD_V_RES
 */
static void display_get_dimensions(uint16_t *w, uint16_t *h)
{
    //IF option("esp32_s3_lcd_ev_board")
    if (w) *w = 480;
    if (h) *h = 480;
    //ELIF option("esp32_s3_box_3")
    if (w) *w = 320;
    if (h) *h = 240;
    //ELIF option("esp32s3_korvo_2")
    if (w) *w = 320;
    if (h) *h = 240;
    //ELIF option("esp32_s3_box")
    if (w) *w = 320;
    if (h) *h = 240;
    //ELIF option("m5stack_core_2")
    if (w) *w = 320;
    if (h) *h = 240;
    //ELIF option("m5stack_core_s3")
    if (w) *w = 320;
    if (h) *h = 240;
    //ELIF option("esp_vocat")
    if (w) *w = 360;
    if (h) *h = 360;
    //ELIF option("esp32_s3_eye")
    if (w) *w = 240;
    if (h) *h = 240;
    //ELIF option("m5stack_atom_s3")
    if (w) *w = 128;
    if (h) *h = 128;
    //ELIF option("m5stack_atom_s3r")
    if (w) *w = 128;
    if (h) *h = 128;
    //ENDIF
}

// External rcore callback
extern void raylib_esp_set_display_callbacks(
    void (*flush_fn)(const uint16_t *buf, uint16_t x, uint16_t y, uint16_t w, uint16_t h),
    void (*get_dim_fn)(uint16_t *w, uint16_t *h)
);

#define RAYLIB_TASK_STACK_SIZE (128 * 1024)

/**
 * @brief Initialize display using BSP (or direct esp_lcd for esp32_s3_box)
 */
static esp_err_t init_display(void)
{
    ESP_LOGI(TAG, "Initializing display...");

    //IF option("esp32_s3_box")
    // ESP32-S3-BOX has no BSP component - use direct esp_lcd init with hardcoded ST7789 pins
    // Pinout from esp-bsp/bsp/esp-box/
    #define ESP32_S3_BOX_LCD_MOSI      GPIO_NUM_6
    #define ESP32_S3_BOX_LCD_SCLK      GPIO_NUM_7
    #define ESP32_S3_BOX_LCD_CS        GPIO_NUM_5
    #define ESP32_S3_BOX_LCD_DC        GPIO_NUM_4
    #define ESP32_S3_BOX_LCD_RST       GPIO_NUM_48
    #define ESP32_S3_BOX_LCD_BACKLIGHT GPIO_NUM_45

    // SPI bus configuration (ESP-IDF 6 API)
    spi_bus_config_t bus_cfg = {
        .sclk_io_num = ESP32_S3_BOX_LCD_SCLK,
        .mosi_io_num = ESP32_S3_BOX_LCD_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 320 * 48 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    // LCD IO SPI configuration (ESP-IDF 6 API)
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = ESP32_S3_BOX_LCD_CS,
        .dc_gpio_num = ESP32_S3_BOX_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = {
            .dc_low_on_data = 0,  // DC=0 for data (ST7789 specific)
        },
    };

    // Create panel IO (ESP-IDF 6 API - uses SPI3_HOST directly)
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_cfg, &g_io));

    // ST7789 panel configuration (ESP-IDF 6 API)
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = ESP32_S3_BOX_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,  // ST7789 expects BGR
        .bits_per_pixel = 16,
    };

    // Initialize ST7789 panel
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(g_io, &panel_cfg, &g_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(g_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(g_panel));
    // Mirror both X and Y for correct orientation (upside-down fix)
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(g_panel, true, true));

    // Turn on display
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(g_panel, true));

    // Turn on backlight
    gpio_config_t bl_cfg = {
        .pin_bit_mask = (1ULL << ESP32_S3_BOX_LCD_BACKLIGHT),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bl_cfg));
    gpio_set_level(ESP32_S3_BOX_LCD_BACKLIGHT, 1);

    //ELIF option("m5stack_atom_s3r")
    // M5Stack AtomS3R display driver changed from GC9107 to ST7735 (2026-05-14)
    // ST7735 not in ESP-IDF, using ST7789 (similar Sitronix chip)
    // Pinout from M5Stack AtomS3R hardware docs
    #define M5STACK_ATOM_S3R_LCD_MOSI      GPIO_NUM_21
    #define M5STACK_ATOM_S3R_LCD_SCLK      GPIO_NUM_15
    #define M5STACK_ATOM_S3R_LCD_CS        GPIO_NUM_14
    #define M5STACK_ATOM_S3R_LCD_DC        GPIO_NUM_42
    #define M5STACK_ATOM_S3R_LCD_RST       GPIO_NUM_48

    // SPI bus configuration (ESP-IDF 6 API)
    spi_bus_config_t bus_cfg = {
        .sclk_io_num = M5STACK_ATOM_S3R_LCD_SCLK,
        .mosi_io_num = M5STACK_ATOM_S3R_LCD_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 128 * 128 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    // LCD IO SPI configuration (ESP-IDF 6 API)
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = M5STACK_ATOM_S3R_LCD_CS,
        .dc_gpio_num = M5STACK_ATOM_S3R_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = {
            .dc_low_on_data = 0,
        },
    };

    // Create panel IO (ESP-IDF 6 API - uses SPI3_HOST directly)
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_cfg, &g_io));

    // ST7789 panel configuration (ESP-IDF 6 API)
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = M5STACK_ATOM_S3R_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };

    // Initialize ST7789 panel
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(g_io, &panel_cfg, &g_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(g_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(g_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(g_panel, true, true));

    // Turn on display
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(g_panel, true));

    //ELIF option("esp32_s3_lcd_ev_board")
    bsp_display_config_t cfg = {
        .max_transfer_sz = 480 * 48 * sizeof(uint16_t),  // 48-line chunks
    };
    esp_err_t ret = bsp_display_new(&cfg, &g_panel, &g_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP display init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    esp_lcd_panel_disp_on_off(g_panel, true);
    bsp_display_backlight_on();
    //ELIF option("m5stack_atom_s3")
    bsp_display_config_t cfg = {
        .max_transfer_sz = 128 * 128 * sizeof(uint16_t),  // Full frame
    };
    esp_err_t ret = bsp_display_new(&cfg, &g_panel, &g_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP display init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    esp_lcd_panel_disp_on_off(g_panel, true);
    bsp_display_backlight_on();
    //ELSE
    bsp_display_config_t cfg = {
        .max_transfer_sz = 320 * 48 * sizeof(uint16_t),  // 48-line chunks
    };
    esp_err_t ret = bsp_display_new(&cfg, &g_panel, &g_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP display init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    esp_lcd_panel_disp_on_off(g_panel, true);
    bsp_display_backlight_on();
    //ENDIF

    // Small delay for display to stabilize
    vTaskDelay(pdMS_TO_TICKS(100));

    // Register display callbacks with rcore
    raylib_esp_set_display_callbacks(display_flush, display_get_dimensions);

    uint16_t w, h;
    display_get_dimensions(&w, &h);
    ESP_LOGI(TAG, "Display initialized: %dx%d", w, h);
    return ESP_OK;
}

void raylib_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Initializing Raylib...");

    uint16_t w, h;
    display_get_dimensions(&w, &h);
    //IF option("esp32_s3_box_3")
    InitWindow(w, h, "ESP32-S3-BOX-3 Raylib");
    //ELIF option("esp32_s3_lcd_ev_board")
    InitWindow(w, h, "ESP32-S3-LCD-EV Raylib");
    //ELIF option("esp32s3_korvo_2")
    InitWindow(w, h, "ESP32-S3-Korvo-2 Raylib");
    //ELIF option("esp32_s3_box")
    InitWindow(w, h, "ESP32-S3-BOX Raylib");
    //ELIF option("esp_vocat")
    InitWindow(w, h, "ESP-VoCat Raylib");
    //ELIF option("esp32_s3_eye")
    InitWindow(w, h, "ESP32-S3-EYE Raylib");
    //ELIF option("m5stack_core_2")
    InitWindow(w, h, "M5Stack-Core2 Raylib");
    //ELIF option("m5stack_atom_s3")
    InitWindow(w, h, "M5Stack-AtomS3 Raylib");
    //ELIF option("m5stack_atom_s3r")
    InitWindow(w, h, "M5Stack-AtomS3R Raylib");
    //ELIF option("m5stack_core_s3")
    InitWindow(w, h, "M5Stack-CoreS3 Raylib");
    //ENDIF

    ESP_LOGI(TAG, "Starting demo loop...");

    // Color test
    BeginDrawing();
    ClearBackground(RED);
    DrawText("RED", 10, 10, 20, WHITE);
    EndDrawing();
    vTaskDelay(pdMS_TO_TICKS(1000));

    BeginDrawing();
    ClearBackground(GREEN);
    DrawText("GREEN", 10, 10, 20, WHITE);
    EndDrawing();
    vTaskDelay(pdMS_TO_TICKS(1000));

    BeginDrawing();
    ClearBackground(BLUE);
    DrawText("BLUE", 10, 10, 20, WHITE);
    EndDrawing();
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Main loop
    int frame = 0;
    while (!WindowShouldClose()) {
        BeginDrawing();

        if ((frame / 60) % 2 == 0)
            ClearBackground(SKYBLUE);
        else
            ClearBackground(LIGHTGRAY);

        int x = frame % w;
        DrawRectangle(x, 30, 50, 50, RED);

        //IF option("esp32_s3_box_3")
        const char *msg = "ESP32-S3-BOX-3";
        //ELIF option("esp32_s3_lcd_ev_board")
        const char *msg = "ESP32-S3-LCD-EV";
        //ELIF option("esp32s3_korvo_2")
        const char *msg = "ESP32-S3-Korvo-2";
        //ELIF option("esp32_s3_box")
        const char *msg = "ESP32-S3-BOX";
        //ELIF option("esp_vocat")
        const char *msg = "ESP-VoCat";
        //ELIF option("esp32_s3_eye")
        const char *msg = "ESP32-S3-EYE";
        //ELIF option("m5stack_core_2")
        const char *msg = "M5Stack-Core2";
        //ELIF option("m5stack_atom_s3")
        const char *msg = "M5Stack-AtomS3";
        //ELIF option("m5stack_atom_s3r")
        const char *msg = "M5Stack-AtomS3R";
        //ELIF option("m5stack_core_s3")
        const char *msg = "M5Stack-CoreS3";
        //ENDIF

        int sz = 20;
        int tw = MeasureText(msg, sz);
        DrawText(msg, (w - tw) / 2, (h - sz) / 2, sz, BLACK);

        DrawRectangle(0, 0, 20, 20, YELLOW);
        DrawRectangle(w - 20, 0, 20, 20, ORANGE);
        DrawRectangle(0, h - 20, 20, 20, PURPLE);
        DrawRectangle(w - 20, h - 20, 20, 20, PINK);

        EndDrawing();
        frame++;
    }

    CloseWindow();
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Raylib Demo");

    ESP_ERROR_CHECK(init_display());

    xTaskCreatePinnedToCore(raylib_task, "raylib", RAYLIB_TASK_STACK_SIZE, NULL, 5, NULL, 1);
}
