/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration for the flash dispatcher task.
 * The dispatcher serializes flash operations to a dedicated task.
 */
typedef struct {
    uint32_t task_stack_size;   // Stack size for the dedicated flash task (in bytes)
    uint32_t task_priority;     // Priority for the dedicated flash task
    BaseType_t task_core_id;    // Core affinity (PRO_CPU_NUM, APP_CPU_NUM, or tskNO_AFFINITY)
    uint32_t queue_size;        // Length of the request queue
} esp_flash_dispatcher_config_t;

/**
 * @brief Default configuration to init flash dispatcher
 */
#define ESP_FLASH_DISPATCHER_DEFAULT_CONFIG   {   \
    .task_stack_size = 2048,                       \
    .task_priority = 10,                  \
    .task_core_id = tskNO_AFFINITY,                      \
    .queue_size = 1,                              \
}

/**
 * @brief Initialize flash dispatcher.
 *
 * @param[in]  cfg: Configuration structure
 *
 * @return
 *      - ESP_OK            on success
 *      - ESP_ERR_NO_MEM    if there is no memory for allocating main structure
 *      - ESP_ERR_INVALID_STATE    if the dispatcher is already initialized
 */
esp_err_t esp_flash_dispatcher_init(const esp_flash_dispatcher_config_t *cfg);

#ifdef __cplusplus
}
#endif


