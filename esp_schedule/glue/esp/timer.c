/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_timer.c
 * @brief ESP Timer implementation.
 */

#include "glue_timer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

/* Types **********************************************************************/

/**
 * @brief Timer private data.
 */
typedef struct {
    esp_schedule_timer_cb_t cb;
    void *priv_data;
} __timer_priv_data_t;

/* Private functions **********************************************************/

/**
 * @brief Create a new timer private data.
 *
 * @param[in] cb Callback function.
 * @param[in] priv_data Private data.
 *
 * @return Pointer to the timer private data.
 */
static __timer_priv_data_t *__timer_priv_data_create(esp_schedule_timer_cb_t cb, void *priv_data)
{
    __timer_priv_data_t *data = calloc(1, sizeof(__timer_priv_data_t));
    if (data == NULL) {
        return NULL;
    }
    data->cb = cb;
    data->priv_data = priv_data;
    return data;
}

/**
 * @brief Get the timer private data from the timer handle.
 *
 * @param[in] timer_handle Timer handle.
 *
 * @return Pointer to the timer private data.
 */
static __timer_priv_data_t *__timer_priv_data_get(esp_schedule_timer_handle_t timer_handle)
{
    return (__timer_priv_data_t *) pvTimerGetTimerID(timer_handle);
}

/**
 * @brief Common timer callback.
 *
 * @param[in] timer_handle Timer handle.
 */
static void __timer_common_cb(TimerHandle_t timer_handle)
{
    __timer_priv_data_t *timer_priv_data = __timer_priv_data_get(timer_handle);
    if (timer_priv_data != NULL) {
        timer_priv_data->cb(timer_priv_data->priv_data);
    }
}

/* Public functions ************************************************************/

void esp_schedule_timer_start(esp_schedule_timer_handle_t *p_timer_handle, uint32_t delay_seconds, esp_schedule_timer_cb_t cb, void *priv_data)
{
    TimerHandle_t timer_handle = (TimerHandle_t) * p_timer_handle;
    if (timer_handle != NULL) {
        esp_schedule_timer_cancel(p_timer_handle);
    }

    __timer_priv_data_t *timer_priv_data = __timer_priv_data_create(cb, priv_data);
    if (timer_priv_data == NULL) {
        return;
    }
    timer_handle = xTimerCreate("schedule", ( ((uint64_t) delay_seconds) * 1000 ) / portTICK_PERIOD_MS, pdFALSE, timer_priv_data, __timer_common_cb);
    if (timer_handle == NULL) {
        free(timer_priv_data);
        return;
    }
    xTimerStart(timer_handle, portMAX_DELAY);
    *p_timer_handle = (esp_schedule_timer_handle_t) timer_handle;
}

void esp_schedule_timer_reset(esp_schedule_timer_handle_t timer_handle, uint32_t delay_seconds)
{
    if (timer_handle == NULL) {
        return;
    }
    xTimerChangePeriod((TimerHandle_t) timer_handle, ( ((uint64_t) delay_seconds) * 1000 ) / portTICK_PERIOD_MS, portMAX_DELAY);
}

void esp_schedule_timer_stop(esp_schedule_timer_handle_t timer_handle)
{
    if (timer_handle == NULL) {
        return;
    }
    xTimerStop((TimerHandle_t) timer_handle, portMAX_DELAY);
}

void esp_schedule_timer_cancel(esp_schedule_timer_handle_t *p_timer_handle)
{
    if (p_timer_handle == NULL || *p_timer_handle == NULL) {
        return;
    }
    TimerHandle_t timer_handle = (TimerHandle_t) * p_timer_handle;
    if (xTimerIsTimerActive(timer_handle) == pdTRUE) {
        /* Stop the timer */
        xTimerStop(timer_handle, portMAX_DELAY);
    }
    /* Free the private data regardless of active state */
    __timer_priv_data_t *priv_data = __timer_priv_data_get(timer_handle);
    if (priv_data != NULL) {
        free(priv_data);
        vTimerSetTimerID(timer_handle, NULL);
    }
    xTimerDelete(timer_handle, portMAX_DELAY);
    *p_timer_handle = NULL;
}