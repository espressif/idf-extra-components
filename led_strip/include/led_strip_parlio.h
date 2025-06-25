/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/parlio_tx.h"
#include "driver/parlio_types.h"
#include "led_strip_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED Strip PARLIO specific configuration
 */
typedef struct {
    parlio_clock_source_t clk_src; /*!< PARLIO clock source */
    uint8_t strip_count;              /*!< Number of LED strips. Should be a power of 2 and not larger than PARLIO_TX_UNIT_MAX_DATA_WIDTH */
    gpio_num_t strip_gpio_num[PARLIO_TX_UNIT_MAX_DATA_WIDTH];   /*!< GPIO number that used by LED strip */
} led_strip_parlio_config_t;

/**
 * @brief Create LED strip group based on PARLIO_TX unit
 *
 * @note The strip_gpio_num in led_config no longer takes effect, and other configurations will be shared by all LED strips in the group.
 *
 * @param led_config LED strip configuration
 * @param parlio_config PARLIO specific configuration
 * @param ret_group Returned LED strip group handle
 * @return
 *      - ESP_OK: create LED strip handle successfully
 *      - ESP_ERR_INVALID_ARG: create LED strip handle failed because of invalid argument
 *      - ESP_ERR_NOT_SUPPORTED: create LED strip handle failed because of unsupported configuration
 *      - ESP_ERR_NO_MEM: create LED strip handle failed because of out of memory
 */
esp_err_t led_strip_new_parlio_group(const led_strip_config_t *led_config, const led_strip_parlio_config_t *parlio_config, led_strip_group_handle_t *ret_group);

#ifdef __cplusplus
}
#endif
