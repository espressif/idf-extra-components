/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include "unity.h"
#include "unity_test_runner.h"
#include "esp_newlib.h"
#include "unity_test_utils_memory.h"
#include "test_xymodem_common.h"

void setUp(void)
{
    xymodem_test_uart_init();
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    esp_reent_cleanup();
    unity_utils_evaluate_leaks_direct(0);
}

void app_main(void)
{
    printf("Running xymodem component tests\n");
    unity_run_menu();
}
