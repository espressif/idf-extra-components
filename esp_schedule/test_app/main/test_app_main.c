/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_schedule_internal.h"

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

    // Initialize NVS for schedules (use default partition)
    ESP_SCHEDULE_RETURN_TYPE nvs_init_result = esp_schedule_nvs_init("nvs", NULL);
    if (nvs_init_result != ESP_SCHEDULE_RET_OK) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(ESP_SCHEDULE_RET_OK, nvs_init_result, "NVS initialization should succeed");
    }

    // Check if NVS is enabled
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_nvs_is_enabled(), "NVS should be enabled");
}

void setUp(void) {}

void tearDown(void) {}

void app_main(void)
{
    printf("Running esp_schedule component tests\n");
    init_nvs_for_tests();
    unity_run_menu();
}
