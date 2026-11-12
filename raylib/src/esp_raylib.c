/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_raylib_private.h"

esp_raylib_display_t *g_esp_raylib_display;

esp_raylib_display_handle_t esp_raylib_add_display(const esp_raylib_display_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "display config is required");
        return NULL;
    }
    if (config->flush_cb == NULL) {
        ESP_LOGE(TAG, "Invalid display config: flush_cb is required");
        return NULL;
    }
    if (g_esp_raylib_display != NULL) {
        ESP_LOGE(TAG, "only one display is currently supported");
        return NULL;
    }

    /* The ISR reads accept_ready and gives the semaphore: keep both in
     * internal RAM so the callback is cache-safe. */
    esp_raylib_display_t *d = heap_caps_calloc(1, sizeof(esp_raylib_display_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (d == NULL) {
        ESP_LOGE(TAG, "no internal RAM for display");
        return NULL;
    }

    d->flush_cb = config->flush_cb;
    d->user_data = config->user_data;
    d->chunk_rows = config->chunk_rows;
    d->swap_bytes = config->swap_bytes;
    d->mirror = config->mirror;
    d->flush_ready = xSemaphoreCreateCountingStatic(ESP_RAYLIB_FLUSH_CREDITS, 0,
                                                    &d->flush_ready_storage);
    g_esp_raylib_display = d;
    return d;
}

IRAM_ATTR
void esp_raylib_flush_ready(esp_raylib_display_handle_t display)
{
    if ((display == NULL) || !display->accept_ready) {
        return;
    }
    BaseType_t wakeup = pdFALSE;
    if (portCHECK_IF_IN_ISR()) {
        xSemaphoreGiveFromISR(display->flush_ready, &wakeup);
        /* Wake the present task now; a tick's delay idles the wire per band. */
        portYIELD_FROM_ISR(wakeup);
    } else {
        xSemaphoreGive(display->flush_ready);
    }
}
