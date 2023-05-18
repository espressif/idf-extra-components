/*
 * SPDX-FileCopyrightText: 2020-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief This helper function creates and starts a task which wraps `tud_task()`.
 *
 * The wrapper function basically wraps tud_task and some log.
 * If you have more requirements for this task, you can create your own task which calls tud_task as the last step.
 *
 * @param usStackDepth - Stack size of crated background tinyusb task
 * @param uxPriority  - Task priority of created background tinyusb task
 * @param xCoreID  - Core on which background tinyusb task will run or tskNO_AFFINITY
 *
 * @retval ESP_OK run tinyusb main task successfully
 * @retval ESP_FAIL run tinyusb main task failed of internal error
 * @retval ESP_ERR_INVALID_STATE tinyusb main task has been created before
 */
esp_err_t tusb_run_task(const size_t usStackDepth, const size_t uxPriority, const BaseType_t xCoreID);

/**
 * @brief This helper function stops and destroys the task created by `tusb_run_task()`
 *
 * @retval ESP_OK stop and destory tinyusb main task successfully
 * @retval ESP_ERR_INVALID_STATE tinyusb main task hasn't been created yet
 */
esp_err_t tusb_stop_task(void);

#ifdef __cplusplus
}
#endif
