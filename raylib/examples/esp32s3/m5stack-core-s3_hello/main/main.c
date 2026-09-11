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

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "raylib.h"

static const char *TAG = "M5STACK_CORES3";

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
    if (w) *w = 320;
    if (h) *h = 240;
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
    InitWindow(w, h, "M5Stack-CoreS3 Raylib");

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

        const char *msg = "M5Stack-CoreS3";

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
