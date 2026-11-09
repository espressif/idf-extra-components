/**********************************************************************************************
*
*   rcore_esp_idf - Raylib platform layer for ESP-IDF
*
*   Simplified architecture - uses callback functions provided by application
*
**********************************************************************************************/

#include "config.h"
#include "raylib.h"
#include "rlgl.h"
#include <string.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

//----------------------------------------------------------------------------------
// POSIX stubs for embedded (weak symbols - overridden by ESP-IDF VFS when present)
//----------------------------------------------------------------------------------

// access() stub - always fail (no POSIX access control on ESP32)
__attribute__((weak)) int access(const char *path, int mode) {
    (void)path;
    (void)mode;
    return -1;
}

// mkdir() stub - always fail (use ESP-IDF VFS if needed)
__attribute__((weak)) int mkdir(const char *path, unsigned int mode) {
    (void)path;
    (void)mode;
    return -1;
}

//----------------------------------------------------------------------------------
// Types and Structures
//----------------------------------------------------------------------------------
typedef struct {
    int placeholder;
} PlatformData;

static PlatformData platform = { 0 };

//----------------------------------------------------------------------------------
// Display Callbacks (set by application)
//----------------------------------------------------------------------------------
static void (*s_display_flush)(const uint16_t *buf, uint16_t x, uint16_t y, uint16_t w, uint16_t h) = NULL;
static void (*s_get_dimensions)(uint16_t *w, uint16_t *h) = NULL;

//----------------------------------------------------------------------------------
// Framebuffer
//----------------------------------------------------------------------------------
static uint16_t *s_framebuffer = NULL;
static int s_screen_width = 0;
static int s_screen_height = 0;
static SemaphoreHandle_t s_flush_mutex = NULL;

// External software renderer API
extern void *swGetColorBuffer(int *width, int *height);

// RLSW state structure
extern struct {
    struct {
        void *pixels;
        int format;
        int width, height;
    } *colorBuffer;
} RLSW;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

// Check if application should close
bool WindowShouldClose(void)
{
    return false;  // Embedded: run continuously
}

// Simple display callback registration (called by app)
void raylib_esp_set_display_callbacks(
    void (*flush_fn)(const uint16_t *buf, uint16_t x, uint16_t y, uint16_t w, uint16_t h),
    void (*get_dim_fn)(uint16_t *w, uint16_t *h)
)
{
    s_display_flush = flush_fn;
    s_get_dimensions = get_dim_fn;
}

// Swap back buffer with front buffer (screen drawing)
void SwapScreenBuffer(void)
{
    if (!s_framebuffer || !s_display_flush) {
        ESP_LOGE("RAYLIB", "Framebuffer or flush callback not set!");
        return;
    }

    // Get software renderer framebuffer (R5G6B5: 2 bytes per pixel)
    int sw_width, sw_height;
    uint16_t *sw_buf = (uint16_t *)swGetColorBuffer(&sw_width, &sw_height);

    if (!sw_buf || sw_width != s_screen_width || sw_height != s_screen_height) {
        ESP_LOGE("RAYLIB", "Framebuffer mismatch! sw=%dx%d vs %dx%d", sw_width, sw_height, s_screen_width, s_screen_height);
        return;
    }

    // Vertical flip for LCD coordinate system
    for (int row = 0; row < s_screen_height; row++) {
        int src_row = s_screen_height - 1 - row;
        memcpy(s_framebuffer + (row * s_screen_width),
               sw_buf + (src_row * s_screen_width),
               s_screen_width * sizeof(uint16_t));
    }

    // Thread-safe flush
    if (s_flush_mutex) {
        xSemaphoreTake(s_flush_mutex, portMAX_DELAY);
    }
    s_display_flush(s_framebuffer, 0, 0, s_screen_width, s_screen_height);
    if (s_flush_mutex) {
        xSemaphoreGive(s_flush_mutex);
    }
}

//----------------------------------------------------------------------------------
// Other window functions (stubs)
//----------------------------------------------------------------------------------

void ToggleFullscreen(void) { TRACELOG(LOG_WARNING, "ToggleFullscreen() not available"); }
void ToggleBorderlessWindowed(void) { TRACELOG(LOG_WARNING, "ToggleBorderlessWindowed() not available"); }
void MaximizeWindow(void) { TRACELOG(LOG_WARNING, "MaximizeWindow() not available"); }
void MinimizeWindow(void) { TRACELOG(LOG_WARNING, "MinimizeWindow() not available"); }
void RestoreWindow(void) { TRACELOG(LOG_WARNING, "RestoreWindow() not available"); }
void SetWindowState(unsigned int flags) { TRACELOG(LOG_WARNING, "SetWindowState() not available"); }
void ClearWindowState(unsigned int flags) { TRACELOG(LOG_WARNING, "ClearWindowState() not available"); }
void SetWindowIcon(Image image) { TRACELOG(LOG_WARNING, "SetWindowIcon() not available"); }
void SetWindowIcons(Image *images, int count) { TRACELOG(LOG_WARNING, "SetWindowIcons() not available"); }
void SetWindowTitle(const char *title) { }
void SetWindowPosition(int x, int y) { TRACELOG(LOG_WARNING, "SetWindowPosition() not available"); }
void SetWindowMonitor(int monitor) { TRACELOG(LOG_WARNING, "SetWindowMonitor() not available"); }
void SetWindowMinSize(int width, int height) { }
void SetWindowMaxSize(int width, int height) { }
void SetWindowSize(int width, int height) { TRACELOG(LOG_WARNING, "SetWindowSize() not available"); }
void SetWindowOpacity(float opacity) { TRACELOG(LOG_WARNING, "SetWindowOpacity() not available"); }
void SetWindowFocused(void) { TRACELOG(LOG_WARNING, "SetWindowFocused() not available"); }
void *GetWindowHandle(void) { return NULL; }
int GetMonitorCount(void) { return 1; }
int GetCurrentMonitor(void) { return 0; }
Vector2 GetMonitorPosition(int monitor) { return (Vector2){ 0, 0 }; }
int GetMonitorWidth(int monitor) { return s_screen_width > 0 ? s_screen_width : 320; }
int GetMonitorHeight(int monitor) { return s_screen_height > 0 ? s_screen_height : 240; }
int GetMonitorPhysicalWidth(int monitor) { return 0; }
int GetMonitorPhysicalHeight(int monitor) { return 0; }
int GetMonitorRefreshRate(int monitor) { return 60; }
const char *GetMonitorName(int monitor) { return "ESP-LCD"; }
Vector2 GetWindowPosition(void) { return (Vector2){ 0, 0 }; }
Vector2 GetWindowScaleDPI(void) { return (Vector2){ 1.0f, 1.0f }; }
void SetClipboardText(const char *text) { TRACELOG(LOG_WARNING, "SetClipboardText() not implemented"); }
const char *GetClipboardText(void) { return ""; }
Image GetClipboardImage(void) { return (Image){ 0 }; }
void ShowCursor(void) { }
void HideCursor(void) { }
void EnableCursor(void) { }
void DisableCursor(void) { }

//----------------------------------------------------------------------------------
// Misc
//----------------------------------------------------------------------------------

double GetTime(void)
{
    return 0;
}

void OpenURL(const char *url)
{
    if (strchr(url, '\'') != NULL) TRACELOG(LOG_WARNING, "OpenURL: Invalid URL");
}

//----------------------------------------------------------------------------------
// Input
//----------------------------------------------------------------------------------

int SetGamepadMappings(const char *mappings) { return 0; }
void SetMousePosition(int x, int y) { }
void SetMouseCursor(int cursor) { TRACELOG(LOG_WARNING, "SetMouseCursor() not implemented"); }
const char *GetKeyName(int key) { return ""; }
void PollInputEvents(void) { }

//----------------------------------------------------------------------------------
// Platform Internal Functions
//----------------------------------------------------------------------------------

static bool CreateWindowFramebuffer(int width, int height)
{
    s_screen_width = width;
    s_screen_height = height;

    // Allocate RGB565 framebuffer (prefer PSRAM)
    s_framebuffer = heap_caps_malloc(width * height * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_framebuffer) {
        s_framebuffer = heap_caps_malloc(width * height * sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    if (!s_framebuffer) {
        TRACELOG(LOG_ERROR, "PLATFORM: Failed to allocate framebuffer");
        return false;
    }

    // Init to green
    for (int i = 0; i < width * height; i++) {
        s_framebuffer[i] = 0x07E0;  // RGB565 green
    }

    TRACELOG(LOG_INFO, "PLATFORM: Framebuffer allocated: %dx%d", width, height);
    return true;
}

int InitPlatform(void)
{
    if (!s_get_dimensions) {
        TRACELOG(LOG_ERROR, "PLATFORM: Display callbacks not set! Call raylib_esp_set_display_callbacks() first.");
        return -1;
    }

    uint16_t width, height;
    s_get_dimensions(&width, &height);

    if (width == 0 || height == 0) {
        TRACELOG(LOG_ERROR, "PLATFORM: Invalid dimensions");
        return -1;
    }

    if (!CreateWindowFramebuffer(width, height)) {
        return -1;
    }

    // Create flush mutex
    s_flush_mutex = xSemaphoreCreateMutex();

    TRACELOG(LOG_INFO, "PLATFORM: ESP-IDF initialized (%dx%d)", width, height);
    return 0;
}

void ClosePlatform(void)
{
    if (s_framebuffer) {
        heap_caps_free(s_framebuffer);
        s_framebuffer = NULL;
    }

    if (s_flush_mutex) {
        vSemaphoreDelete(s_flush_mutex);
        s_flush_mutex = NULL;
    }

    TRACELOG(LOG_INFO, "PLATFORM: ESP-IDF closed");
}
