/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "unity_test_runner.h"
#include "esp_heap_caps.h"
#include "unity_test_utils_memory.h"

/* newlib's strtod()/strtof() keep per-task caches (big-number buffers and
 * powers of five) that grow with the exponent and digit count converted.
 * Convert the extremes once here so the leak check below does not see them.
 * esp_reent_cleanup() must not be called between tests: it frees the buffer
 * cache but orphans the powers-of-five chain, so every call leaks and the
 * next conversion rebuilds the caches inside a test. */
static void warm_up_number_conversion(void)
{
    char longest[90] = "0.";
    memset(longest + 2, '0', 80);
    strcpy(longest + 82, "1");
    const char *extremes[] = {
        "1e400", "5e-324", "4.9406564584124654e-324", "1.7976931348623157e308",
        "2.2250738585072014e-308", "123456789012345678901234567890", longest,
    };
    volatile double sink = 0;
    for (size_t i = 0; i < sizeof(extremes) / sizeof(extremes[0]); i++) {
        sink += strtod(extremes[i], NULL);
        sink += strtof(extremes[i], NULL);
    }
    sink += strtof("1e39", NULL);
}

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    unity_utils_evaluate_leaks_direct(0);
}

void app_main(void)
{
    printf("Running json_parser component tests\n");
    warm_up_number_conversion();
    unity_run_menu();
}
