/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_schedule_esp_port.h
 * @brief The ESP-IDF implementations of the esp_schedule port operations.
 *
 * Exposed so a port can be assembled from these rather than written from
 * scratch. Take the tables you want as-is, replace the ones you do not, and
 * install the result with esp_schedule_init_with_config():
 *
 * @code
 * esp_schedule_port_config_t port = {
 *     .timer = esp_schedule_esp_timer_ops,
 *     .nvs   = esp_schedule_esp_nvs_ops,
 *     .time_sync  = esp_schedule_esp_time_sync_ops,
 *     .mem   = esp_schedule_esp_mem_ops,
 *     .log   = esp_schedule_esp_log,
 * };
 * port.time_sync.timesync_init = NULL;      // keep time(), skip starting SNTP
 * port.mem = my_pool_ops;              // or replace a whole group
 * esp_schedule_init_with_config(&port, true, NULL, &count);
 * @endcode
 *
 * Individual members may be overridden as above, but only where the porting
 * interface says the member is optional. @c time_sync.get_time and every member of
 * @c timer and @c mem are required, and esp_schedule_init_with_config() rejects
 * a table that is missing one.
 *
 * @note Including this header is harmless. Referencing a table pulls the
 *       corresponding object under port/esp into the link, and with it that
 *       implementation's dependency - FreeRTOS timers, nvs_flash, esp_sntp or
 *       esp_log. That is the intended trade: you pay only for the tables you
 *       name. An application that names none of them, and calls
 *       esp_schedule_init_with_config() rather than esp_schedule_init(), links
 *       none of the default port at all.
 *
 * @note To keep time() without starting SNTP across the whole build rather than
 *       per-port, CONFIG_ESP_SCHEDULE_ENABLE_SNTP=n does it in the default
 *       table and drops the esp_sntp dependency outright. Nulling
 *       @c timesync_init stops the call but still links esp_sntp, because
 *       naming @c esp_schedule_esp_time_sync_ops links port/esp/time.c.
 */

#pragma once

#include "esp_schedule.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Timer operations backed by FreeRTOS software timers. See port/esp/timer.c. */
extern const esp_schedule_timer_ops_t esp_schedule_esp_timer_ops;

/** @brief Storage operations backed by nvs_flash. See port/esp/nvs.c. */
extern const esp_schedule_nvs_ops_t esp_schedule_esp_nvs_ops;

/** @brief Wall-clock operations backed by time()/SNTP. See port/esp/time.c. */
extern const esp_schedule_time_sync_ops_t esp_schedule_esp_time_sync_ops;

/** @brief Heap operations backed by the libc allocator. See port/esp/mem.c. */
extern const esp_schedule_mem_ops_t esp_schedule_esp_mem_ops;

/**
 * @brief Emit one pre-formatted log line through esp_log.
 *
 * Maps @c level onto the matching ESP_LOGx macro. A plain function rather than
 * an ops table because esp_schedule_log_fn_t is a bare function pointer.
 *
 * @param[in] level Severity of the line.
 * @param[in] tag Log tag.
 * @param[in] message NUL-terminated, already-formatted message.
 */
void esp_schedule_esp_log(esp_schedule_log_level_t level, const char *tag,
                          const char *message);

#ifdef __cplusplus
}
#endif
