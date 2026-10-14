/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Lua Blink Example
 *
 * This example demonstrates how to embed Lua in an ESP-IDF application:
 *
 *   - one long-lived lua_State, which is how real firmware uses Lua
 *   - firmware APIs (free heap, LED strip, sleep) exposed as C modules that scripts load with require()
 *   - a script loaded from LittleFS that drives the LED strip
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_littlefs.h"

#include "lua.h"
#include "lauxlib.h"

#include "app_lua.h"

#define TAG "lua_blink"
#define LUA_FILE_PATH "/assets"

#define BLINK_LED_SCRIPT "blink_led.lua"

// Function to log memory usage
static void log_memory_usage(const char *message)
{
    ESP_LOGI(TAG, "Free heap: %zu, Min free heap: %zu, Largest free block: %zu, %s",
             heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
             heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT),
             heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT),
             message);
}

// Initialize and mount the filesystem. The partition image is flashed with
// the project, so do not format on mount failure: that would wipe the scripts.
static void init_filesystem(void)
{
    ESP_LOGI(TAG, "Initializing LittleFS filesystem");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = LUA_FILE_PATH,
        .partition_label = "assets",
    };

    ESP_ERROR_CHECK(esp_vfs_littlefs_register(&conf));
    ESP_LOGI(TAG, "Filesystem mounted at %s", LUA_FILE_PATH);
}

/* Function to run a Lua script from file */
static void run_lua_file(lua_State *L, const char *file_name)
{
    ESP_LOGI(TAG, "Starting Lua script from file: %s", file_name);

    // Construct the full file path
    char full_path[128];
    snprintf(full_path, sizeof(full_path), LUA_FILE_PATH"/%s", file_name);

    if (luaL_dofile(L, full_path) != LUA_OK) {
        ESP_LOGE(TAG, "Error running %s: %s", file_name,
                 (lua_type(L, -1) == LUA_TSTRING) ? lua_tostring(L, -1) : "(non-string error)");
    }
    /* Drop the error object or leftover return values so a later run starts clean. */
    lua_settop(L, 0);

    ESP_LOGI(TAG, "End of Lua script from file: %s", file_name);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Lua Blink Example Starting");

    init_filesystem();

    led_strip_handle_t led_strip;
    app_lua_init_led_strip(&led_strip);

    log_memory_usage("Before luaL_newstate");
    lua_State *L = luaL_newstate();
    ESP_ERROR_CHECK(L != NULL ? ESP_OK : ESP_FAIL);
    log_memory_usage("After luaL_newstate");

    /* The application decides what the script may require(): open the
     * libraries (including package), point package.path at the script
     * directory, then preload one C module per topic. */
    app_lua_open_libs(L);
    app_lua_set_search_path(L, LUA_FILE_PATH);
    app_lua_register_esp(L);
    app_lua_register_led_strip(L, led_strip);

    run_lua_file(L, BLINK_LED_SCRIPT);

    log_memory_usage("After running script");

    lua_close(L);
    app_lua_deinit_led_strip(led_strip);
    log_memory_usage("After lua_close");

    ESP_LOGI(TAG, "End of Lua blink example application.");

    // Prevent the task from ending
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
