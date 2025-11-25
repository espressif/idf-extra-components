/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "unity_test_runner.h"
#include "esp_heap_caps.h"
#include "unity_test_utils_memory.h"

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    /* the threshold is necessary because on esp_linenoise instance
     * creation, a bunch of heap memory is being used to initialize (e.g.,
     * eventfd and vfs internals) */
    unity_utils_evaluate_leaks_direct(500);
}

void app_main(void)
{
    printf("Running esp_cli component tests\n");
    unity_run_menu();
}
