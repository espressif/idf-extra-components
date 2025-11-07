/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_time.c
 * @brief ESP implementation of the time interface for the esp_schedule component
 */

#include "glue_time.h"
#include <esp_sntp.h>

time_t esp_schedule_get_time(time_t *p_time)
{
    return time(p_time);
}

void esp_schedule_timesync_init(void)
{
    if (!esp_sntp_enabled()) {
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_init();
    }
}