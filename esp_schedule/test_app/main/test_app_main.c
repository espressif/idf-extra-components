/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "unity.h"
#include "esp_err.h"
#include "esp_schedule_internal.h"
#if CONFIG_ESP_SCHEDULE_ENABLE_NVS
#include "nvs_flash.h"

// NVS initialization for tests
static void init_nvs_for_tests(void)
{
    // Initialize NVS flash storage with specific partition
    esp_err_t err = nvs_flash_init_partition("nvs");
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated or has new version - erase and reinitialize
        ESP_ERROR_CHECK(nvs_flash_erase_partition("nvs"));
        err = nvs_flash_init_partition("nvs");
    }
    ESP_ERROR_CHECK(err);

    // Initialize NVS for schedules (no private-data callbacks in these tests)
    esp_err_t nvs_init_result = esp_schedule_nvs_init("nvs", NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ESP_OK, nvs_init_result, "NVS initialization should succeed");

    // Check if NVS is enabled
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_nvs_is_enabled(), "NVS should be enabled");
}
#endif /* CONFIG_ESP_SCHEDULE_ENABLE_NVS */

void app_main(void)
{
    printf("Running esp_schedule component tests\n");
#if CONFIG_ESP_SCHEDULE_ENABLE_NVS
    init_nvs_for_tests();
#endif
    unity_run_menu();
}
