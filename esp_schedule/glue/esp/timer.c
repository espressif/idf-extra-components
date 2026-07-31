/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_timer.c
 * @brief ESP Timer implementation.
 */

#include "glue_timer.h"
#include "glue_log.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

static const char *TAG = "esp_schedule_timer";

/* Types **********************************************************************/

/* xTimerCreate()/xTimerChangePeriod() take a 32-bit TickType_t, so a period
 * derived from a delay beyond ~49.7 days (at CONFIG_FREERTOS_HZ=1000) would
 * truncate and fire early. Cap a single hardware period to a safe value and
 * split longer delays across several periods, re-arming without invoking the
 * user callback until the full delay has elapsed. portMAX_DELAY/2 leaves
 * headroom and avoids the portMAX_DELAY "block forever" sentinel. */
#define ESP_SCHEDULE_TIMER_MAX_TICKS ((TickType_t)(portMAX_DELAY / 2))

/**
 * @brief Timer private data.
 */
typedef struct {
    esp_schedule_timer_cb_t cb;
    void *priv_data;
    uint64_t remaining_ticks; /* ticks still to wait after the current period */
} __timer_priv_data_t;

/* Callback serialization ******************************************************
 *
 * FreeRTOS software-timer callbacks run in the timer service (daemon) task,
 * while esp_schedule_timer_cancel() runs in the caller's (app) task. xTimerStop
 * only prevents FUTURE expiries; it does not wait for a callback that is already
 * executing. Without synchronization, cancel() would free() the priv_data (and
 * the caller then frees the schedule) while __timer_common_cb is still
 * dereferencing them in the daemon task -> use-after-free.
 *
 * A single global mutex serializes the callback body against cancel(). The
 * callback holds it while running (including across the user trigger callback);
 * cancel() waits on it as a barrier so any in-flight callback has fully
 * completed before priv_data is freed. cancel() never holds the mutex while
 * calling a blocking timer API, so a full timer-command queue cannot deadlock
 * the daemon against the app task.
 *
 * The mutex is RECURSIVE. The user trigger callback runs with it held, and from
 * there the user may legitimately re-enter this layer on the same task: a
 * repeating schedule re-arms itself via esp_schedule_timer_start(), and a
 * one-shot schedule that deletes itself reaches esp_schedule_timer_cancel().
 * With a non-recursive mutex either re-entry would block the daemon task on a
 * mutex the daemon itself holds, freezing every software timer in the system.
 * Recursive ownership makes the nested take a no-op for the owning task while
 * still serializing app tasks against an in-flight callback. */
static portMUX_TYPE s_cb_mutex_init_lock = portMUX_INITIALIZER_UNLOCKED;
static StaticSemaphore_t s_cb_mutex_buf;
static SemaphoreHandle_t s_cb_mutex = NULL;

static void __ensure_cb_mutex(void)
{
    if (s_cb_mutex != NULL) {
        return;
    }
    /* Static allocation performs no heap work, so it is safe inside a critical
     * section; the lock makes first-time creation race-free. */
    portENTER_CRITICAL(&s_cb_mutex_init_lock);
    if (s_cb_mutex == NULL) {
        s_cb_mutex = xSemaphoreCreateRecursiveMutexStatic(&s_cb_mutex_buf);
    }
    portEXIT_CRITICAL(&s_cb_mutex_init_lock);
}

/**
 * @brief Whether the caller is running in the FreeRTOS timer daemon task.
 */
static bool __in_timer_daemon(void)
{
    return xTaskGetCurrentTaskHandle() == xTimerGetTimerDaemonTaskHandle();
}

/**
 * @brief Block time to use for a timer command queue operation.
 *
 * FreeRTOS forbids a non-zero block time on the timer command queue from within
 * a timer callback: if the queue is full the daemon task would end up waiting
 * for itself to drain it. Every entry point here is reachable from the daemon
 * task via the user trigger callback, so the block time must be chosen from the
 * calling context rather than hardcoded.
 */
static TickType_t __cmd_block_ticks(void)
{
    return __in_timer_daemon() ? 0 : portMAX_DELAY;
}

/* Private functions **********************************************************/

/**
 * @brief Convert a delay in seconds to timer ticks (64-bit to avoid overflow).
 */
static uint64_t __ticks_from_seconds(uint32_t delay_seconds)
{
    return (((uint64_t) delay_seconds) * 1000) / portTICK_PERIOD_MS;
}

/**
 * @brief Clamp a 64-bit tick count to a valid single timer period (>0).
 */
static TickType_t __period_ticks(uint64_t total_ticks)
{
    if (total_ticks == 0) {
        return 1; /* a timer period must be strictly greater than 0 */
    }
    if (total_ticks > ESP_SCHEDULE_TIMER_MAX_TICKS) {
        return ESP_SCHEDULE_TIMER_MAX_TICKS;
    }
    return (TickType_t) total_ticks;
}

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
 * @brief Common timer callback. Runs in the FreeRTOS timer daemon task.
 *
 * @param[in] timer_handle Timer handle.
 */
static void __timer_common_cb(TimerHandle_t timer_handle)
{
    __ensure_cb_mutex();
    /* Hold the mutex for the whole callback so cancel() can barrier against it.
     * priv_data is read AFTER acquiring the mutex: cancel() clears the timer ID
     * under the same mutex before freeing, so we either see a valid pointer
     * (cancel has not started) or NULL (cancel already ran). */
    xSemaphoreTakeRecursive(s_cb_mutex, portMAX_DELAY);
    __timer_priv_data_t *timer_priv_data = __timer_priv_data_get(timer_handle);
    if (timer_priv_data == NULL) {
        xSemaphoreGiveRecursive(s_cb_mutex);
        return;
    }
    if (timer_priv_data->remaining_ticks > 0) {
        /* A long delay split across multiple periods: keep counting down and
         * re-arm without invoking the user callback yet. A timer callback must
         * never block on the timer command queue, so use a zero block time.
         *
         * Only commit the countdown once the re-arm has actually been queued: a
         * full command queue would otherwise consume this slice of the delay
         * with no timer left running to serve it, and the schedule would
         * silently never fire. */
        TickType_t period = __period_ticks(timer_priv_data->remaining_ticks);
        if (xTimerChangePeriod(timer_handle, period, 0) == pdPASS) {
            timer_priv_data->remaining_ticks -= period;
        } else {
            ESP_SCHEDULE_LOGE(TAG, "Failed to re-arm timer; schedule is now disarmed");
        }
        xSemaphoreGiveRecursive(s_cb_mutex);
        return;
    }
    timer_priv_data->cb(timer_priv_data->priv_data);
    xSemaphoreGiveRecursive(s_cb_mutex);
}

/* Public functions ************************************************************/

bool esp_schedule_timer_start(esp_schedule_timer_handle_t *p_timer_handle, uint32_t delay_seconds, esp_schedule_timer_cb_t cb, void *priv_data)
{
    __ensure_cb_mutex();
    TimerHandle_t timer_handle = (TimerHandle_t) * p_timer_handle;

    uint64_t total_ticks = __ticks_from_seconds(delay_seconds);
    TickType_t period = __period_ticks(total_ticks);
    uint64_t remaining_ticks = (total_ticks > period) ? (total_ticks - period) : 0;

    if (timer_handle != NULL) {
        /* Reuse the existing timer rather than cancel + recreate. A repeating
         * schedule re-arms from inside its own fired callback (daemon task);
         * routing that through esp_schedule_timer_cancel() would delete the
         * timer that is currently executing and free the priv_data the callback
         * is still using.
         *
         * Instead, rebind the private data in place: this honors the start()
         * contract (the caller-provided cb and priv_data are set explicitly,
         * not assumed unchanged) without freeing anything the running callback
         * references, and avoids per-fire create/delete churn.
         *
         * The mutex serializes the rebind against a callback running
         * concurrently on the daemon, which could otherwise read a half-updated
         * (cb, priv_data) pair. Re-arming from inside the fired callback takes
         * it recursively on the task that already owns it. */
        xSemaphoreTakeRecursive(s_cb_mutex, portMAX_DELAY);
        __timer_priv_data_t *timer_priv_data = __timer_priv_data_get(timer_handle);
        if (timer_priv_data == NULL) {
            /* A concurrent cancel() detached and is deleting this timer; there
             * is nothing safe to reuse. */
            xSemaphoreGiveRecursive(s_cb_mutex);
            return false;
        }
        timer_priv_data->cb = cb;
        timer_priv_data->priv_data = priv_data;
        timer_priv_data->remaining_ticks = remaining_ticks;
        xSemaphoreGiveRecursive(s_cb_mutex);

        /* xTimerChangePeriod also (re)starts a dormant/expired timer. A dropped
         * command leaves the schedule disarmed, so report it rather than
         * claiming success. */
        return xTimerChangePeriod(timer_handle, period, __cmd_block_ticks()) == pdPASS;
    }

    __timer_priv_data_t *timer_priv_data = __timer_priv_data_create(cb, priv_data);
    if (timer_priv_data == NULL) {
        return false;
    }
    timer_priv_data->remaining_ticks = remaining_ticks;
    timer_handle = xTimerCreate("schedule", period, pdFALSE, timer_priv_data, __timer_common_cb);
    if (timer_handle == NULL) {
        free(timer_priv_data);
        return false;
    }
    /* A trigger callback may create and enable a brand new schedule, so this
     * path is reachable from the daemon task too. */
    if (xTimerStart(timer_handle, __cmd_block_ticks()) != pdPASS) {
        /* Detach before deleting: if the delete command itself cannot be queued
         * the timer outlives this call, and a later expiry must find a NULL ID
         * rather than the freed private data. */
        vTimerSetTimerID(timer_handle, NULL);
        xTimerDelete(timer_handle, __cmd_block_ticks());
        free(timer_priv_data);
        return false;
    }
    *p_timer_handle = (esp_schedule_timer_handle_t) timer_handle;
    return true;
}

void esp_schedule_timer_stop(esp_schedule_timer_handle_t timer_handle)
{
    if (timer_handle == NULL) {
        return;
    }
    xTimerStop((TimerHandle_t) timer_handle, __cmd_block_ticks());
}

void esp_schedule_timer_cancel(esp_schedule_timer_handle_t *p_timer_handle)
{
    if (p_timer_handle == NULL || *p_timer_handle == NULL) {
        return;
    }
    TimerHandle_t timer_handle = (TimerHandle_t) * p_timer_handle;
    __ensure_cb_mutex();

    /* Reachable from the user trigger callback (a one-shot schedule that
     * deletes itself when it fires), so the timer command queue must not be
     * blocked on when running on the daemon task. */
    TickType_t block_ticks = __cmd_block_ticks();

    /* Stop the timer so no new expiry is dispatched. */
    if (xTimerIsTimerActive(timer_handle) == pdTRUE) {
        xTimerStop(timer_handle, block_ticks);
    }

    /* Detach the private data under the callback mutex. Taking the mutex is the
     * barrier: any callback already executing on another task runs to
     * completion before we proceed, and any callback dispatched afterwards will
     * read a NULL timer ID and return without touching the freed data. When the
     * caller IS the running callback the mutex is taken recursively and there is
     * nothing to barrier against. The mutex is released before the
     * xTimerDelete()/free() so a full timer-command queue cannot deadlock the
     * daemon against this task. */
    __timer_priv_data_t *priv_data = __timer_priv_data_get(timer_handle);
    xSemaphoreTakeRecursive(s_cb_mutex, portMAX_DELAY);
    if (priv_data != NULL) {
        vTimerSetTimerID(timer_handle, NULL);
    }
    xSemaphoreGiveRecursive(s_cb_mutex);

    xTimerDelete(timer_handle, block_ticks);
    if (priv_data != NULL) {
        free(priv_data);
    }
    *p_timer_handle = NULL;
}
