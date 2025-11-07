/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file glue_time.h
 * @brief Time interface for the esp_schedule component
 */

#ifndef __GLUE_TIME_H__
#define __GLUE_TIME_H__

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the current time.
 * @param[out] p_time Pointer to a time_t variable to store the current time.
 * @return The current time.
 */
time_t esp_schedule_get_time(time_t *p_time);

/**
 * @brief Initialize the timesync.
 */
void esp_schedule_timesync_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __GLUE_TIME_H__ */