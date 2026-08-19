/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "unity.h"
#include "unity_test_utils.h"
#include "esp_heap_caps.h"
#include "esp_newlib.h"
#include "unity_test_utils_memory.h"
#include "test_spi_nand_common.h"

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    /* Unity aborts on assertion failure and skips in-test deinit; free SPI here. */
    spi_nand_flash_test_teardown();
    esp_reent_cleanup();    //clean up some of the newlib's lazy allocations
    unity_utils_evaluate_leaks_direct(32);
}

void app_main(void)
{
    printf("Running spi_nand_flash component tests\n");
    unity_run_menu();
}
