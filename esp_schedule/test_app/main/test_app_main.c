/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include "unity.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_schedule_internal.h"
#include "esp_schedule_esp_port.h"

/* See sdkconfig.defaults: the tests pin the clock, and this app installs a port
 * with timesync_init NULL while tests call esp_schedule_init() themselves. Both
 * tables must agree on that member or the second install is rejected. */
#if CONFIG_ESP_SCHEDULE_ENABLE_SNTP
#error "test_app requires CONFIG_ESP_SCHEDULE_ENABLE_SNTP=n - see sdkconfig.defaults"
#endif

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

    /* Install the ESP-IDF port. Everything in the component now reaches timers,
     * storage, time, the heap and the log through it, so this has to happen
     * before any other esp_schedule call - including the internal NVS helpers
     * the tests use directly.
     *
     * Installed through esp_schedule_init_with_config() rather than
     * esp_schedule_init() so that time sync can be left out. The tests pin the
     * clock with settimeofday(); an SNTP sync completing mid-test would move it
     * under them, which is latent flakiness on any DUT that has a network. Not
     * starting SNTP also means no TCP/IP stack is needed here.
     *
     * This doubles as the in-tree exercise of the custom-port entry point: the
     * same tables the default port installs, with one operation replaced.
     *
     * That replacement is only safe because SNTP is disabled for this app, which
     * makes it a no-op. Several tests call esp_schedule_init() themselves, and
     * esp_schedule_port_equal() compares timesync_init: if the default table
     * carried a non-NULL one, the port installed here would differ from the one
     * those calls build, the second install would be rejected with
     * ESP_ERR_INVALID_STATE, and esp_schedule_init() would return NULL without
     * restoring anything from NVS. The tests that depend on that restore would
     * fail, and the ones that only assert NULL would pass for the wrong reason.
     * Hence the guard above rather than a comment. */
    esp_schedule_port_config_t port = {
        .timer = esp_schedule_esp_timer_ops,
        .nvs = esp_schedule_esp_nvs_ops,
        .time_sync = esp_schedule_esp_time_sync_ops,
        .mem = esp_schedule_esp_mem_ops,
        .log = esp_schedule_esp_log,
    };
    port.time_sync.timesync_init = NULL;

    uint8_t restored = 0;
    esp_schedule_handle_t *handles = esp_schedule_init_with_config(&port, true, "nvs", &restored);

    // Check if NVS is enabled
    TEST_ASSERT_TRUE_MESSAGE(esp_schedule_nvs_is_enabled(), "NVS should be enabled");

    /* Start every run from an empty namespace, whatever a previous run left
     * behind, so per-test count assertions are exact. */
    for (uint8_t i = 0; i < restored; i++) {
        esp_schedule_delete(handles[i]);
    }
    /* The array came from port.mem.malloc, so it goes back the same way. */
    ESP_SCHEDULE_FREE(handles);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ESP_OK, esp_schedule_nvs_remove_all(), "NVS reset should succeed");
}

void app_main(void)
{
    printf("Running esp_schedule component tests\n");
    init_nvs_for_tests();
    unity_run_menu();
}
