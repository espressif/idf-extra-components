/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Flushes buffered events without taking the encoder lock.
 *
 * @return ESP_OK on success, an error code otherwise. Buffered events are kept on error.
 */
esp_err_t SEGGER_RTT_ESP_FlushNoLock(void);

/**
 * @brief Flushes buffered events.
 *
 * @return ESP_OK on success, an error code otherwise.
 */
esp_err_t SEGGER_RTT_ESP_Flush(void);

#ifdef __cplusplus
}
#endif
