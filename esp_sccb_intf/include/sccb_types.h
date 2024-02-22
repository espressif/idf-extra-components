/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "hal/gpio_types.h"
#include "driver/i2c_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief sccb io handle type
 */
typedef struct sccb_io_t* sccb_io_handle_t;

#ifdef __cplusplus
}
#endif
