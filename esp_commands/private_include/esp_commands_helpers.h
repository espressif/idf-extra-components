/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/**
 * @brief Split a command line and populate argc and argv parameters
 *
 * @param line the line that has to be split into arguments
 * @param argv array of arguments created from the line
 * @param argv_size size of the argument array
 * @return size_t number of arguments found in the line and stored
 * in argv
 */
size_t esp_commands_split_argv(char *line, char **argv, size_t argv_size);

#ifdef __cplusplus
}
#endif
