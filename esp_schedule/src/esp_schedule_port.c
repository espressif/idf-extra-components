/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_schedule_port.c
 * @brief Holds the installed port and validates it at install time.
 *
 * @note Nothing here may reference the ESP-IDF implementations in port/esp/.
 *       They are reachable only from esp_schedule_init() in
 *       src/esp_schedule_default.c, which is what keeps them out of the link
 *       for anyone who only calls esp_schedule_init_with_config(). See the
 *       comment at the top of that file.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "esp_schedule_internal.h"

static const char *TAG = "esp_schedule_port";

esp_schedule_port_config_t g_esp_schedule_port;

static bool s_port_installed = false;

bool esp_schedule_is_inited(void)
{
    return s_port_installed;
}

/* Field-wise rather than memcmp: the struct has padding whose contents are
 * unspecified even for a designated initializer, so a byte comparison of two
 * logically identical configs can differ. */
static bool esp_schedule_port_equal(const esp_schedule_port_config_t *a,
                                    const esp_schedule_port_config_t *b)
{
    return a->timer.start == b->timer.start
           && a->timer.stop == b->timer.stop
           && a->timer.cancel == b->timer.cancel
           && a->nvs.open == b->nvs.open
           && a->nvs.close == b->nvs.close
           && a->nvs.read == b->nvs.read
           && a->nvs.write == b->nvs.write
           && a->nvs.erase == b->nvs.erase
           && a->nvs.foreach_key == b->nvs.foreach_key
           && a->time_sync.get_time == b->time_sync.get_time
           && a->time_sync.timesync_init == b->time_sync.timesync_init
           && a->mem.malloc == b->mem.malloc
           && a->mem.calloc == b->mem.calloc
           && a->mem.free == b->mem.free
           && a->log == b->log;
}

esp_err_t esp_schedule_port_install(const esp_schedule_port_config_t *port)
{
    if (port == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_port_installed) {
        /* Re-initialising with the very same port is a no-op, so that calling
         * esp_schedule_init() more than once stays harmless. Swapping in a
         * different one is not: the table is read without locking everywhere
         * else precisely because it never changes after init, and schedules
         * already allocated hold timers from the old port. */
        if (esp_schedule_port_equal(&g_esp_schedule_port, port)) {
            return ESP_OK;
        }
        ESP_SCHEDULE_LOGE(TAG, "A different port is already installed; cannot replace it");
        return ESP_ERR_INVALID_STATE;
    }

    /* Validate once here so that no call site has to NULL-check. The log
     * operation is deliberately not required: a NULL log is a silent build.
     * It is also installed before the validation failures below are reported,
     * so those reports are not lost. */
    g_esp_schedule_port.log = port->log;

    if (port->timer.start == NULL || port->timer.stop == NULL || port->timer.cancel == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Port is missing one or more timer operations");
        return ESP_ERR_INVALID_ARG;
    }
    if (port->time_sync.get_time == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Port is missing time_sync.get_time");
        return ESP_ERR_INVALID_ARG;
    }
    if (port->mem.malloc == NULL || port->mem.calloc == NULL || port->mem.free == NULL) {
        ESP_SCHEDULE_LOGE(TAG, "Port is missing one or more memory operations");
        return ESP_ERR_INVALID_ARG;
    }

    /* Storage is optional, but it is all-or-nothing: a partially filled table
     * would fail at some unpredictable later point instead of here. */
    const esp_schedule_nvs_ops_t *nvs = &port->nvs;
    int nvs_set = (nvs->open != NULL) + (nvs->close != NULL) + (nvs->read != NULL)
                  + (nvs->write != NULL) + (nvs->erase != NULL) + (nvs->foreach_key != NULL);
    if (nvs_set != 0 && nvs_set != 6) {
        ESP_SCHEDULE_LOGE(TAG, "Port supplies an incomplete set of storage operations");
        return ESP_ERR_INVALID_ARG;
    }

    g_esp_schedule_port = *port;
    s_port_installed = true;
    return ESP_OK;
}

/* Deliberately small: this frequently runs on the timer daemon's stack, so the
 * buffer truncates rather than grows. Every call site in the component is
 * written to fit, worst-case name length included - the longest expands to 152
 * of the 159 usable bytes. A new message longer than that will be silently cut,
 * so keep them within it rather than raising this. */
#define ESP_SCHEDULE_LOG_BUF_LEN 160

void esp_schedule_log(esp_schedule_log_level_t level, const char *tag, const char *format, ...)
{
    esp_schedule_log_fn_t log = g_esp_schedule_port.log;
    if (log == NULL) {
        return;
    }
    /* Format here rather than handing the va_list to the port. Ownership of a
     * va_list is easy to get wrong across an interface boundary - an
     * implementation that called va_end() on it would double-end this one,
     * which is undefined - and doing it here also spares every port a varargs
     * printf. vsnprintf always NUL-terminates within the buffer. */
    char message[ESP_SCHEDULE_LOG_BUF_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    /* clang-analyzer-valist.Uninitialized reports this va_end() as operating on
     * a list that was never started. It is a checker regression in clang 21
     * (what CI runs), not a defect here: the emitted path goes straight from the
     * `log == NULL` branch above to this line without ever modelling the
     * va_start() two lines up, and clang 19 does not report it. The pairing is
     * unconditional and immediate, with only vsnprintf() - which consumes but
     * does not end the list - in between. */
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    va_end(args);

    log(level, tag, message);
}
