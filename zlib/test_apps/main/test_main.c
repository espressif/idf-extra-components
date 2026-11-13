/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "esp_heap_caps.h"
#if !CONFIG_IDF_TARGET_LINUX
#include "esp_newlib.h"     // chip targets only: the host libc has no newlib reent
#endif
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    printf("\r\n");
#if !CONFIG_IDF_TARGET_LINUX
    esp_reent_cleanup();    //clean up some of the newlib's lazy allocations
#endif
    unity_utils_evaluate_leaks_direct(0);
}

void app_main(void)
{
    printf("Running zlib component tests\n");
    unity_run_menu();
}
