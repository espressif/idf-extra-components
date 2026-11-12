/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * ESP-IDF platform port for raylib's software renderer.
 *
 * This file is not compiled on its own. CMake copies upstream rcore.c into a
 * shadow directory so its quoted #include "platforms/rcore_memory.c" resolves
 * against that copy; the shadow stub is one line that includes this file by
 * absolute path. Upstream selects PLATFORM_MEMORY (GRAPHICS=Software) and
 * includes that stub after rcore.c's declarations and before its function
 * bodies, so this code shares rcore.c's translation unit: CORE, the config.h
 * limits, rlsw and the platform entry-point declarations are all already in
 * scope. The #define wrappers below (rlglClose, rlViewport, rlScissor) then
 * rewrite every later call site in rcore.c. See CMakeLists.txt.
 *
 * Lifecycle. add_display() is process-lifetime (CloseWindow does not undo it).
 * Indentation is the call stack; a later sibling runs only after the previous
 * sibling returns. Arrows are the present_task rendezvous.
 *
 *   app: panel + flush_cb
 *   app: esp_raylib_add_display()          display, flush_ready
 *   app: InitWindow()
 *     rcore: screen = (w, h)
 *     InitPlatform()
 *       currentFbo / display / basePath / clock
 *       present_start()
 *         color_free, present_q, gone
 *         esp_raylib_seed()                accept_ready = true
 *         bands (chunk_rows > 0)
 *         xTaskCreate(present_task)  ---->  recv present_q
 *     rcore: render = screen (if still 0)
 *     rcore: rlglInit()                    color buffers
 *
 *   app: CloseWindow()                     [render task]
 *     rcore: UnloadFontDefault()
 *     rcore: rlglClose()                   [#define below reorders this]
 *       present_quiesce()                  DMA must finish before swClose
 *         drain present_q                  drop frames never started
 *         send quit  ------------------>  present_task
 *         wait gone                       recv quit
 *           (blocked)                       wait last DMA
 *           (blocked)                       accept_ready = false
 *         <----------------------------   give gone, vTaskDelete
 *       swClose()                          color buffers (now idle)
 *     ClosePlatform()
 *       present_teardown()                 queues, semaphores, bands
 *     rcore: ready = false
 *
 * Frame hand-off: EndDrawing() -> SwapScreenBuffer() queues the finished
 * color, waits color_free, then swSwapColorBuffers(). present_task owns
 * DMA; esp_raylib_flush_ready() (ISR-safe) reports each transfer. InitWindow
 * seeds one credit; issue_flush starts the next before waiting the previous,
 * so up to two transfers are in flight.
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_raylib.h"
#include "esp_raylib_private.h"

#define FRAME_BPP         SW_FRAMEBUFFER_COLOR_SIZE
#define FRAME_IS_RGB565   (SW_FRAMEBUFFER_COLOR_FORMAT == SW_PIXELFORMAT_COLOR_R5G6B5)

/* EndDrawing() measures the frame as update + draw and calls WaitTime() when
 * that lands below the target. On ESP-IDF none of WaitTime()'s sleep branches
 * compile (they are all platform-locked to Win32/POSIX/Apple/Emscripten),
 * and SUPPORT_PARTIALBUSY_WAIT_LOOP defaults to 1, so WaitTime() degenerates
 * into a pure spin loop that also starves the idle task. We therefore pace in
 * SwapScreenBuffer(), against the same span upstream measures, so the frame
 * WaitTime() sees is never below the target and that spin stays dormant.
 * Two microseconds of margin absorb GetTime()'s microsecond quantisation. */
#define RAYLIB_PACE_MARGIN_S   0.000002

typedef struct {
    void *pixels;
    uint16_t width;
    uint16_t height;
} present_req_t;

typedef struct {
    SemaphoreHandle_t color_free;
    QueueHandle_t present_q;
    SemaphoreHandle_t gone;
    TaskHandle_t present_task;

    /* Full-frame path only: issue_flush() returns when the previous frame's
     * DMA has finished, so a color buffer is recycled one frame after it was
     * submitted. Only the very first frame has nothing to recycle yet. */
    bool full_started;

    /* Two staging bands owned by the present task, handed to flush_cb in
     * turn. After a flush_cb the task waits flush_ready (the previous
     * transfer), then fills the other band while DMA still reads the one it
     * just submitted. */
    uint8_t *bands[2];
    uint16_t band_rows;
    uint8_t band_cur;
} platform_t;

static platform_t platform;

static void present_task(void *arg);

/* Free present_start() allocations. The task must already have exited
 * (present_quiesce), except on present_start() failure where it never ran. */
static void present_teardown(void)
{
    if (platform.color_free != NULL) {
        vSemaphoreDelete(platform.color_free);
        platform.color_free = NULL;
    }
    if (platform.present_q != NULL) {
        vQueueDelete(platform.present_q);
        platform.present_q = NULL;
    }
    if (platform.gone != NULL) {
        vSemaphoreDelete(platform.gone);
        platform.gone = NULL;
    }
    free(platform.bands[0]);
    free(platform.bands[1]);
    platform.bands[0] = NULL;
    platform.bands[1] = NULL;

    platform.present_task = NULL;
    platform.band_cur = 0;
    platform.full_started = false;
}

/* Called once by InitPlatform(); a failure fails InitWindow(), so the frame
 * path never allocates or retries anything. */
static bool present_start(void)
{
    platform.color_free = xSemaphoreCreateCounting(ESP_RAYLIB_COLOR_CREDITS, 1);
    platform.present_q = xQueueCreate(2, sizeof(present_req_t));
    platform.gone = xSemaphoreCreateBinary();
    if ((platform.color_free == NULL) || (platform.present_q == NULL) ||
            (platform.gone == NULL)) {
        goto fail;
    }

    esp_raylib_display_t *d = esp_raylib_current();

    /* Drop stale completions from a previous window, then one seed so the
     * wait after the first flush_cb returns immediately. */
    esp_raylib_seed(d);
    platform.band_cur = 0;
    platform.full_started = false;

    /* Internal RAM keeps the panel's DMA source cheap and DMA-capable.
     * chunk_rows == 0 skips staging: flush_cb receives the color buffer. */
    platform.band_rows = d->chunk_rows;
    if (platform.band_rows > CORE.Window.screen.height) {
        /* A band taller than the screen is never sent whole, so the extra
         * rows would be internal RAM for nothing. */
        platform.band_rows = CORE.Window.screen.height;
    }
    if (platform.band_rows != 0) {
        const size_t band_size = (size_t)CORE.Window.screen.width * FRAME_BPP * platform.band_rows;
        platform.bands[0] = heap_caps_malloc(band_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        platform.bands[1] = heap_caps_malloc(band_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if ((platform.bands[0] == NULL) || (platform.bands[1] == NULL)) {
            ESP_LOGE(TAG, "no internal RAM for two %u-row bands", (unsigned)platform.band_rows);
            goto fail;
        }
    }

    /* Slightly above the renderer helps keep the panel fed. A renderer that
     * already sits at the top priority keeps the present task beside it
     * rather than below it. */
    UBaseType_t prio = uxTaskPriorityGet(NULL) + 1;
    if (prio >= configMAX_PRIORITIES) {
        prio = configMAX_PRIORITIES - 1;
    }
    if (xTaskCreate(present_task, "raylib_present", ESP_RAYLIB_PRESENT_STACK_SIZE,
                    NULL, prio, &platform.present_task) != pdPASS) {
        goto fail;
    }
    return true;

fail:
    ESP_LOGE(TAG, "could not start the present task");
    present_teardown();
    return false;
}

/* Start this transfer, then wait for the previous one (or the InitWindow
 * seed). This flush stays in DMA while the caller writes the next buffer.
 *
 * Ordering matters: esp_lcd must be allowed to queue the next draw_bitmap
 * while the previous DMA is still running (trans_queue_depth >= 2), and
 * flush_ready reports completions in the order transfers were started (see
 * esp_raylib.h). With that pairing the wait below consumes the completion
 * of the transfer issued one call earlier, never the one just started. */
static void issue_flush(esp_raylib_display_t *d, const void *pixels,
                        uint16_t y, uint16_t rows, uint16_t width)
{
    d->flush_cb(d, pixels, 0, y, width, rows, d->user_data);
    esp_raylib_wait(d);
}

static void recycle_colour(void)
{
    xSemaphoreGive(platform.color_free);
}

/* Copy a contiguous run of color-buffer rows into an internal-RAM band for
 * DMA. RGB565 rows are optionally byte-swapped; Y order is not changed. */
static void send_band(esp_raylib_display_t *d, const uint8_t *src,
                      uint16_t y, uint16_t rows, uint16_t width)
{
    uint8_t *band = platform.bands[platform.band_cur];
    uint8_t *dst = band;
    const size_t row_len = (size_t)width * FRAME_BPP;
    const bool swap = FRAME_IS_RGB565 && d->swap_bytes;

    if (swap) {
        /* bswap32 consumes two pixels per word, so the width has to be even
         * (InitPlatform() refuses an odd width before a frame is drawn). An
         * even width also makes row_len a multiple of four, which keeps every
         * source row and band row 4-byte aligned for the uint32_t access.
         * A little-endian word holds [lo hi lo hi]; bswap32 reverses all four
         * bytes and the 16-bit rotate puts the two pixels back in order,
         * leaving both byte-swapped. */
        const uint8_t *row = src;
        for (uint16_t i = 0; i < rows; i++) {
            const uint32_t *in32 = (const uint32_t *)row;
            uint32_t *out32 = (uint32_t *)dst;
            for (uint16_t x = 0; x < width / 2; x++) {
                uint32_t v = __builtin_bswap32(in32[x]);
                out32[x] = (v >> 16) | (v << 16);
            }
            dst += row_len;
            row += row_len;
        }
    } else {
        memcpy(dst, src, row_len * rows);
    }

    issue_flush(d, band, y, rows, width);
    platform.band_cur ^= 1;
}

/* Owns the panel hand-off. After every flush_cb the task waits flush_ready
 * (first: seed; later: previous transfer done), then produces the next
 * buffer while this one is still being read:
 *   full-frame: free the color submitted one frame ago (its DMA finished
 *     with the wait inside this frame's issue_flush); the frame just
 *     submitted stays on the wire until the next frame's wait.
 *   bands: the other staging slot is free to fill; after the last band the
 *     color has been fully copied (last DMA is on a staging band), so free
 *     this color immediately. */
static void present_task(void *arg)
{
    (void)arg;
    esp_raylib_display_t *d = esp_raylib_current();
    present_req_t req;

    for (;;) {
        xQueueReceive(platform.present_q, &req, portMAX_DELAY);
        if (req.pixels == NULL) {   /* quit sentinel from present_quiesce() */
            /* Last issue_flush left its transfer in DMA; wait for it, then
             * ignore further flush_ready so a late ISR cannot credit the
             * next InitWindow. */
            esp_raylib_wait(d);
            d->accept_ready = false;
            break;
        }

        if (platform.band_rows == 0) {
            /* flush_cb handed the color buffer itself to DMA, with no copy:
             * it stays owned by the panel until a later wait reports it done.
             * issue_flush() below returns once the previous frame's DMA has
             * completed, so that previous color buffer is the one to free. */
            issue_flush(d, req.pixels, 0, req.height, req.width);
            if (platform.full_started) {
                recycle_colour();
            }
            platform.full_started = true;
        } else {
            const size_t row_bytes = (size_t)req.width * FRAME_BPP;
            for (uint16_t y = 0; y < req.height; y += platform.band_rows) {
                const uint16_t rows = (uint16_t)((y + platform.band_rows > req.height)
                                                 ? (req.height - y) : platform.band_rows);
                const uint8_t *src = (const uint8_t *)req.pixels + (size_t)y * row_bytes;
                send_band(d, src, y, rows, req.width);
            }
            recycle_colour();
        }
    }

    xSemaphoreGive(platform.gone);
    vTaskDelete(NULL);
}

void SwapScreenBuffer(void)
{
    /* swGetColorBuffer() returns the default framebuffer, independent of any
     * RenderTexture bound by BeginTextureMode(). */
    int fb_width = 0, fb_height = 0;
    void *pixels = swGetColorBuffer(&fb_width, &fb_height);
    if ((pixels == NULL) || (fb_width <= 0) || (fb_height <= 0)) {
        ESP_LOGE(TAG, "renderer has no color buffer");
        return;
    }

    /* Hand the finished frame to the present task, wait until the other
     * color is free, then make it the draw target. Same order as send_band:
     * issue_flush waits for the previous DMA, then band_cur flips to the
     * slot that wait just released. color_free is one credit per free color
     * buffer; rlsw owns the two-buffer alternation, the credit only paces. */
    const present_req_t req = {
        .pixels = pixels,
        .width = (uint16_t)fb_width,
        .height = (uint16_t)fb_height,
    };
    xQueueSend(platform.present_q, &req, portMAX_DELAY);
    xSemaphoreTake(platform.color_free, portMAX_DELAY);
    swSwapColorBuffers();

    /* Frame pacing against an absolute deadline:
     *
     * CORE.Time.current is BeginDrawing()'s timestamp and CORE.Time.update
     * the time since the previous frame ended, so
     *
     *     deadline = current + (target - update) = previous frame end + target
     *
     * is the moment the next frame should begin. Waiting on that absolute
     * time (rather than on a relative duration) means a tick-quantised
     * overshoot never accumulates into the next frame's deadline. The wait
     * is a vTaskDelay(), which also feeds this core's idle-task watchdog. */
    const double target = CORE.Time.target;
    const double deadline = CORE.Time.current + (target - CORE.Time.update) + RAYLIB_PACE_MARGIN_S;
    if ((target > 0.0) && (GetTime() < deadline)) {
        while (GetTime() < deadline) {
            vTaskDelay(1);
        }
    } else {
        vTaskDelay(1);
    }
}

bool WindowShouldClose(void)
{
    return !CORE.Window.ready || CORE.Window.shouldClose;
}

void ToggleFullscreen(void) { }
void ToggleBorderlessWindowed(void) { }
void MaximizeWindow(void) { }
void MinimizeWindow(void) { }
void RestoreWindow(void) { }
void SetWindowState(unsigned int flags)
{
    (void)flags;
}
void ClearWindowState(unsigned int flags)
{
    (void)flags;
}
void SetWindowIcon(Image image)
{
    (void)image;
}
void SetWindowIcons(Image *images, int count)
{
    (void)images;
    (void)count;
}
void SetWindowPosition(int x, int y)
{
    (void)x;
    (void)y;
}
void SetWindowMonitor(int monitor)
{
    (void)monitor;
}
void SetWindowMinSize(int width, int height)
{
    (void)width;
    (void)height;
}
void SetWindowMaxSize(int width, int height)
{
    (void)width;
    (void)height;
}
void SetWindowSize(int width, int height)
{
    (void)width;
    (void)height;
}
void SetWindowOpacity(float opacity)
{
    (void)opacity;
}
void SetWindowFocused(void) { }
void *GetWindowHandle(void)
{
    /* The symbol is required by rcore.c; there is no native window to return. */
    return NULL;
}
void SetWindowTitle(const char *title)
{
    CORE.Window.title = title;
}
Vector2 GetWindowPosition(void)
{
    Vector2 pos = { 0.0f, 0.0f };
    return pos;
}
Vector2 GetWindowScaleDPI(void)
{
    Vector2 scale = { 1.0f, 1.0f };
    return scale;
}
int GetMonitorCount(void)
{
    return 1;
}
int GetCurrentMonitor(void)
{
    return 0;
}
Vector2 GetMonitorPosition(int monitor)
{
    (void)monitor;
    Vector2 pos = { 0.0f, 0.0f };
    return pos;
}
int GetMonitorWidth(int monitor)
{
    (void)monitor;
    return CORE.Window.screen.width;
}
int GetMonitorHeight(int monitor)
{
    (void)monitor;
    return CORE.Window.screen.height;
}
int GetMonitorPhysicalWidth(int monitor)
{
    (void)monitor;
    /* Physical size in millimetres: the panel geometry is not known here. */
    return 0;
}
int GetMonitorPhysicalHeight(int monitor)
{
    (void)monitor;
    return 0;
}
int GetMonitorRefreshRate(int monitor)
{
    (void)monitor;
    return 60;
}
const char *GetMonitorName(int monitor)
{
    (void)monitor;
    return "esp_lcd";
}
void SetClipboardText(const char *text)
{
    (void)text;
}
const char *GetClipboardText(void)
{
    return "";
}
Image GetClipboardImage(void)
{
    Image img = { 0 };
    return img;
}
void ShowCursor(void) { }
void HideCursor(void) { }
void EnableCursor(void) { }
void DisableCursor(void) { }
void SetMousePosition(int x, int y)
{
    (void)x;
    (void)y;
}
void SetMouseCursor(int cursor)
{
    (void)cursor;
}
int SetGamepadMappings(const char *mappings)
{
    (void)mappings;
    return 0;
}
void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration)
{
    (void)gamepad;
    (void)leftMotor;
    (void)rightMotor;
    (void)duration;
}
const char *GetKeyName(int key)
{
    (void)key;
    return "";
}

/* No input device of our own. Snapshot previous* so Pressed/Released/Delta
 * work when the app writes CORE.Input.current*; leave current* alone (held
 * state). Wheel is a per-frame delta, so it is copied then cleared. */
void PollInputEvents(void)
{
    CORE.Input.Keyboard.keyPressedQueueCount = 0;
    CORE.Input.Keyboard.charPressedQueueCount = 0;
    CORE.Input.Gamepad.lastButtonPressed = GAMEPAD_BUTTON_UNKNOWN;

    memcpy(CORE.Input.Keyboard.previousKeyState, CORE.Input.Keyboard.currentKeyState,
           sizeof(CORE.Input.Keyboard.previousKeyState));
    memset(CORE.Input.Keyboard.keyRepeatInFrame, 0,
           sizeof(CORE.Input.Keyboard.keyRepeatInFrame));

    memcpy(CORE.Input.Mouse.previousButtonState, CORE.Input.Mouse.currentButtonState,
           sizeof(CORE.Input.Mouse.previousButtonState));
    CORE.Input.Mouse.previousWheelMove = CORE.Input.Mouse.currentWheelMove;
    CORE.Input.Mouse.currentWheelMove = (Vector2) {
        0.0f, 0.0f
    };
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;

    memcpy(CORE.Input.Touch.previousTouchState, CORE.Input.Touch.currentTouchState,
           sizeof(CORE.Input.Touch.previousTouchState));
    memcpy(CORE.Input.Touch.previousPosition, CORE.Input.Touch.position,
           sizeof(CORE.Input.Touch.previousPosition));

    memcpy(CORE.Input.Gamepad.previousButtonState, CORE.Input.Gamepad.currentButtonState,
           sizeof(CORE.Input.Gamepad.previousButtonState));
}

double GetTime(void)
{
    const esp_raylib_display_t *d = esp_raylib_current();
    if (d == NULL) {
        /* Before esp_raylib_add_display() there is no clock to read. */
        return 0.0;
    }
    return (double)(esp_timer_get_time() - d->start_us) / 1000000.0;
}

void OpenURL(const char *url)
{
    ESP_LOGW(TAG, "OpenURL() is not supported on this target: %s", (url != NULL) ? url : "");
}

int InitPlatform(void)
{
    if (rlGetVersion() != RL_OPENGL_SOFTWARE) {
        ESP_LOGE(TAG, "this component only supports the software renderer");
        return -1;
    }
    esp_raylib_display_t *d = esp_raylib_current();
    if (d == NULL) {
        ESP_LOGE(TAG, "call esp_raylib_add_display() before InitWindow()");
        return -1;
    }

    CORE.Window.display.width = CORE.Window.screen.width;
    CORE.Window.display.height = CORE.Window.screen.height;
    CORE.Window.currentFbo.width = CORE.Window.screen.width;
    CORE.Window.currentFbo.height = CORE.Window.screen.height;
    CORE.Storage.basePath = "/";

    /* Upstream anchors its frame clock through InitTimer(), which nothing
     * calls here. GetTime() is our own esp_timer-based clock. */
    d->start_us = esp_timer_get_time();
    CORE.Time.previous = GetTime();

    if (d->swap_bytes && (d->chunk_rows == 0)) {
        ESP_LOGW(TAG, "swap_bytes is ignored when chunk_rows is 0");
    } else if (d->swap_bytes && !FRAME_IS_RGB565) {
        ESP_LOGW(TAG, "swap_bytes is set but the framebuffer is not RGB565; it will be ignored");
    }
    /* The byte swap works on two pixels at a time, so the width has to be
     * even. Refuse it here rather than in the band copy, where an odd width
     * would read past the end of every row. An assert would not do: it is
     * compiled out of release builds. */
    const bool swap_active = d->swap_bytes && (d->chunk_rows != 0) && FRAME_IS_RGB565;
    if (swap_active && ((CORE.Window.screen.width % 2) != 0)) {
        ESP_LOGE(TAG, "swap_bytes needs an even framebuffer width, %d is odd",
                 (int)CORE.Window.screen.width);
        return -1;
    }
    if (!present_start()) {
        return -1;
    }

    CORE.Window.ready = true;
    ESP_LOGI(TAG, "display %ux%u, software renderer, %d bpp framebuffer",
             CORE.Window.screen.width, CORE.Window.screen.height, (int)(FRAME_BPP * 8));
    return 0;
}

/* Stop present_task and wait until its last DMA is done. See file-header
 * CloseWindow diagram: this runs inside the rlglClose wrap, before swClose. */
static void present_quiesce(void)
{
    if (platform.present_task == NULL) {
        return;
    }

    /* Drop frames that were queued but never started. Their color buffers
     * are not recycled: the queues go away with them. */
    present_req_t stale;
    while (xQueueReceive(platform.present_q, &stale, 0) == pdTRUE) {
    }

    const present_req_t quit = { 0 };
    xQueueSend(platform.present_q, &quit, portMAX_DELAY);
    xSemaphoreTake(platform.gone, portMAX_DELAY);
    platform.present_task = NULL;
}

void ClosePlatform(void)
{
    /* Task already gone (present_quiesce). Only the objects it used remain. */
    present_teardown();
}

/* Wrappers. rcore.c includes this file before CloseWindow, BeginTextureMode,
 * BeginScissorMode and SetupViewport, so the macros below rewrite those
 * calls through the rest of the translation unit. */

/* CloseWindow() would otherwise free color buffers (swClose) while DMA still
 * reads them. The wrap is present_quiesce then the real rlglClose; a macro is
 * not re-expanded while expanding. See file-header CloseWindow diagram. */
#define rlglClose() (present_quiesce(), rlglClose())

/* Panel mirror is a scanout transform of the default color buffer.
 * RenderTexture must keep API orientation so DrawTexture / source rects
 * match desktop raylib. */
static bool panel_mirror_applies(void)
{
    return rlGetActiveFramebuffer() == 0;
}

/* After swViewport, NDC +X/+Y still point the OpenGL way (right, up).
 * Negating a vpHalf axis writes the scene mirrored into the color buffer
 * so RGB/MIPI panels that cannot flip in hardware still scan out correctly.
 * Face culling stays in clip space, so winding is unchanged. */
static void esp_rlViewport(int x, int y, int width, int height)
{
    rlViewport(x, y, width, height);
    if (!panel_mirror_applies()) {
        return;
    }
    const unsigned mirror = (unsigned)esp_raylib_current()->mirror;
    if (mirror & ESP_RAYLIB_MIRROR_X) {
        RLSW.vpHalf[0] = -RLSW.vpHalf[0];
    }
    if (mirror & ESP_RAYLIB_MIRROR_Y) {
        RLSW.vpHalf[1] = -RLSW.vpHalf[1];
    }
}
#define rlViewport(x, y, w, h) esp_rlViewport((x), (y), (w), (h))

/* Clip-space scissor is already in NDC (BeginScissorMode did the GL convert).
 * After a vpHalf flip, only the window-space box (glClear / points) must
 * follow the framebuffer; scClip stays. Offscreen viewports are not flipped. */
static void esp_rlScissor(int x, int y, int width, int height)
{
    rlScissor(x, y, width, height);
    if (!panel_mirror_applies()) {
        return;
    }
    const unsigned mirror = (unsigned)esp_raylib_current()->mirror;
    if (mirror & ESP_RAYLIB_MIRROR_Y) {
        const int lo = RLSW.scMin[1];
        const int hi = RLSW.scMax[1];
        RLSW.scMin[1] = RLSW.vpSize[1] - hi;
        RLSW.scMax[1] = RLSW.vpSize[1] - lo;
    }
    if (mirror & ESP_RAYLIB_MIRROR_X) {
        const int lo = RLSW.scMin[0];
        const int hi = RLSW.scMax[0];
        RLSW.scMin[0] = RLSW.vpSize[0] - hi;
        RLSW.scMax[0] = RLSW.vpSize[0] - lo;
    }
}
#define rlScissor(x, y, w, h) esp_rlScissor((x), (y), (w), (h))
