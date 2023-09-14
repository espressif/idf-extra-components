/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: BSL-1.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

/**
 * @brief Register a command to run Catch2 tests.
 *
 * @param cmd_name  Name of the command to use. For example, "test".
 * @return esp_err_t  ESP_OK on success, otherwise an error code.
 */
esp_err_t register_catch2(const char *cmd_name);

#ifdef __cplusplus
}
#endif
