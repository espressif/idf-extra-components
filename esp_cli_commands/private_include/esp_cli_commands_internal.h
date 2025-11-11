/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_heap_caps.h"

/**
 * @brief Component specific implementation of malloc
 *
 * @note This function uses heap_caps_malloc together
 * with the set of capabilities provided by the user in the
 * config structure. Implemented in esp_cli_commands.c
 *
 * @param malloc_size
 * @return void*
 */
void *esp_cli_commands_malloc(const size_t malloc_size);

#ifdef __cplusplus
}
#endif
