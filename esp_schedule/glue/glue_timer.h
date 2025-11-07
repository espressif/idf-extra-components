/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file glue_timer.h
 * @brief Timer interface for the esp_schedule component
 */

#ifndef __GLUE_TIMER_H__
#define __GLUE_TIMER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Types **********************************************************************/

/**
 * @brief Timer handle.
 */
typedef void *esp_schedule_timer_handle_t;

/**
 * @brief Timer callback.
 *
 * @param[in] priv_data Pointer to the private data of the timer when created.
 */
typedef void (*esp_schedule_timer_cb_t)(void *priv_data);

/* Functions ******************************************************************/

/**
 * @brief Start a timer for a schedule.
 *
 * @param[in,out] p_timer_handle Pointer to the timer handle. If NULL, a new timer will be created, else change the period of the existing timer.
 * @param[in] delay_seconds Delay in seconds.
 */
void esp_schedule_timer_start(esp_schedule_timer_handle_t *p_timer_handle, uint32_t delay_seconds, esp_schedule_timer_cb_t cb, void *priv_data);

/**
 * @brief Reset an existing timer for a schedule.
 *
 * This uses the same callback and private data as the existing timer.
 *
 * @param[in] timer_handle Timer handle.
 * @param[in] delay_seconds Delay in seconds.
 */
void esp_schedule_timer_reset(esp_schedule_timer_handle_t timer_handle, uint32_t delay_seconds);

/**
 * @brief Stop a timer for a schedule.
 *
 * @param[in] timer_handle Timer handle.
 */
void esp_schedule_timer_stop(esp_schedule_timer_handle_t timer_handle);

/**
 * @brief Cancel a timer for a schedule, i.e., stop and delete the timer.
 *
 * @param[in,out] p_timer_handle Pointer to the timer handle. Will be set to NULL, and the underlying timer will be deleted.
 */
void esp_schedule_timer_cancel(esp_schedule_timer_handle_t *p_timer_handle);

#ifdef __cplusplus
}
#endif

#endif // __GLUE_TIMER_H__