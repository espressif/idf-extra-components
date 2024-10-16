/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2023 Espressif Systems (Shanghai) CO LTD
 */

#pragma once

#include <stdint.h>
#include "spi_nand_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INVALID_PAGE 0xFFFF

struct spi_nand_flash_device_t {
    spi_nand_flash_config_t config;
    uint32_t block_size;
    uint32_t page_size;
    uint32_t num_blocks;
    struct dhara_map dhara_map;
    struct dhara_nand dhara_nand;
    uint8_t *work_buffer;
    uint8_t *read_buffer;
    uint32_t read_page_delay_us;
    uint32_t erase_block_delay_us;
    uint32_t program_page_delay_us;
    SemaphoreHandle_t mutex;
};

esp_err_t wait_for_ready(spi_device_handle_t device, uint32_t expected_operation_time_us, uint8_t *status_out);

#ifdef __cplusplus
}
#endif
