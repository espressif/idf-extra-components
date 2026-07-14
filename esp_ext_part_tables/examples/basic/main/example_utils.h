/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_ext_part_tables.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t load_first_sector_from_sd_card(void *mbr_buffer);
char *parsed_type_to_str(uint8_t type);

#ifdef __cplusplus
}
#endif
