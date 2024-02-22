/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <esp_types.h>
#include "sdkconfig.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
#include "sccb_io_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------
            Driver Context
---------------------------------------------------------------*/
typedef struct sccb_io_i2c_t sccb_io_i2c_t;

/**
 * @brief sccb i2c controller type
 */
struct sccb_io_i2c_t {
    i2c_master_dev_handle_t i2c_device;
    struct sccb_io_t base;
};

#ifdef __cplusplus
}
#endif
