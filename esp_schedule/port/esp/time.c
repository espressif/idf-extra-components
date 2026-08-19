/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file time.c
 * @brief SNTP implementation of esp_schedule_time_sync_ops_t.
 *
 * @note Reached only through esp_schedule_esp_time_sync_ops, which only
 *       esp_schedule_init() references. A build that installs its own port
 *       never links this file, and with it never pulls in esp_sntp.
 */

#include "esp_schedule.h"
#include "esp_schedule_esp_port.h"

#include <time.h>
#if CONFIG_ESP_SCHEDULE_ENABLE_SNTP
#include <esp_sntp.h>
#endif

static time_t esp_time_get(time_t *p_time)
{
    return time(p_time);
}

#if CONFIG_ESP_SCHEDULE_ENABLE_SNTP
static void esp_time_sync_init(void)
{
    if (!esp_sntp_enabled()) {
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_init();
    }
}
#endif

/* Operations table ************************************************************
 *
 * The only exported symbol in this file. esp_schedule_init() is its sole
 * referrer, so nothing here is linked into a build that installs its own port.
 *
 * timesync_init is NULL when CONFIG_ESP_SCHEDULE_ENABLE_SNTP is disabled, which
 * the core treats as "this platform has no time sync" and skips. That is the
 * supported way to run against an RTC or an externally set clock without
 * writing a port. Compiling the function out with it also drops the esp_sntp
 * reference, so the dependency goes away rather than merely going unused. */
const esp_schedule_time_sync_ops_t esp_schedule_esp_time_sync_ops = {
    .get_time = esp_time_get,
#if CONFIG_ESP_SCHEDULE_ENABLE_SNTP
    .timesync_init = esp_time_sync_init,
#else
    .timesync_init = NULL,
#endif
};
