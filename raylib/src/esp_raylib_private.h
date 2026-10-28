/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_raylib.h"

#define TAG "raylib.idf"

/* Fixed internal default for the frame-present task. */
#define ESP_RAYLIB_PRESENT_STACK_SIZE 4096

/* One in-flight DMA plus the InitWindow seed (or an ISR that beat the wait).
 * A counting semaphore caps at this value, so a duplicate/late give is
 * dropped instead of crediting a buffer the panel still owns. */
#define ESP_RAYLIB_FLUSH_CREDITS 2

/* Two rlsw color buffers. Create with one credit already posted so the
 * renderer can draw the first frame; a duplicate recycle is dropped
 * instead of inventing a third buffer. */
#define ESP_RAYLIB_COLOR_CREDITS 2

/* State behind an esp_raylib_display_handle_t */
typedef struct esp_raylib_display_t {
    esp_raylib_flush_cb_t flush_cb;
    void *user_data;
    uint16_t chunk_rows;
    SemaphoreHandle_t flush_ready;
    StaticSemaphore_t flush_ready_storage;
    int64_t start_us;
    esp_raylib_mirror_t mirror;
    bool swap_bytes;
    /* ISR-visible. flush_ready() is a no-op until present_start() seeds the
     * semaphore, and again after the present task's final wait, so a
     * panel_init transfer or a late DMA cannot credit the next session. */
    volatile bool accept_ready;
} esp_raylib_display_t;

/* Single global display: rcore.c owns one framebuffer and one CORE state. */
extern esp_raylib_display_t *g_esp_raylib_display;

static inline esp_raylib_display_t *esp_raylib_current(void)
{
    return g_esp_raylib_display;
}

/* Start of a lifecycle: drop stale credits, then post one seed so the wait
 * after the first flush_cb returns immediately. */
static inline void esp_raylib_seed(esp_raylib_display_t *d)
{
    d->accept_ready = false;
    while (xSemaphoreTake(d->flush_ready, 0) == pdTRUE) {
    }
    xSemaphoreGive(d->flush_ready);
    d->accept_ready = true;
}

static inline void esp_raylib_wait(esp_raylib_display_t *d)
{
    xSemaphoreTake(d->flush_ready, portMAX_DELAY);
}
