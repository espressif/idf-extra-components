/*
 * Lua Example
 *
 * This example demonstrates how to embed Lua in an ESP-IDF application.
 * It shows both embedded Lua script execution and loading Lua scripts from
 * a filesystem (LittleFS).
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include "esp_vfs.h"
#include <dirent.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#define TAG "lua_example"
#define LUA_FILE_PATH "/assets"

// Function to log memory usage
static void log_memory_usage(const char *message)
{
    ESP_LOGI(TAG, "Free heap: %d, Min free heap: %d, Largest free block: %d, %s",
             heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
             heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT),
             heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT),
             message);
}

// Initialize and mount the filesystem
static void init_filesystem(void)
{
    ESP_LOGI(TAG, "Initializing LittleFS filesystem");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = LUA_FILE_PATH,
        .partition_label = "assets",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format filesystem: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Filesystem mounted at %s", LUA_FILE_PATH);
    }
}

// Function to run a Lua script from file
static void run_lua_file(const char *file_name, const char *test_name)
{
    ESP_LOGI(TAG, "Starting Lua test from file: %s", test_name);

    log_memory_usage("Start of test");

    lua_State *L = luaL_newstate();
    if (L == NULL) {
        ESP_LOGE(TAG, "Failed to create new Lua state");
        return;
    }
    log_memory_usage("After luaL_newstate");

    luaL_openlibs(L);

    // Set the Lua module search path
    if (luaL_dostring(L, "package.path = package.path .. ';./?.lua;/assets/?.lua'")) {
        ESP_LOGE(TAG, "Failed to set package.path: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    log_memory_usage("After luaL_openlibs");

    // Construct the full file path
    char full_path[128];
    snprintf(full_path, sizeof(full_path), LUA_FILE_PATH"/%s", file_name);

    if (luaL_dofile(L, full_path) == LUA_OK) {
        lua_pop(L, lua_gettop(L));
    } else {
        ESP_LOGE(TAG, "Error running Lua script from file '%s': %s", full_path, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    log_memory_usage("After executing Lua script from file");

    lua_close(L);
    log_memory_usage("After lua_close");

    ESP_LOGI(TAG, "End of Lua test from file: %s", test_name);
}

// Function to run an embedded Lua script
static void run_embedded_lua_test(const char *lua_script, const char *test_name)
{
    ESP_LOGI(TAG, "Starting Lua test: %s", test_name);

    log_memory_usage("Start of test");

    lua_State *L = luaL_newstate();
    if (L == NULL) {
        ESP_LOGE(TAG, "Failed to create new Lua state");
        return;
    }
    log_memory_usage("After luaL_newstate");

    luaL_openlibs(L);
    log_memory_usage("After luaL_openlibs");

    if (luaL_dostring(L, lua_script) == LUA_OK) {
        lua_pop(L, lua_gettop(L));
    } else {
        ESP_LOGE(TAG, "Error running embedded Lua script: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    log_memory_usage("After executing Lua script");

    lua_close(L);
    log_memory_usage("After lua_close");

    ESP_LOGI(TAG, "End of Lua test: %s", test_name);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Lua Example Starting");

    // Initialize and mount the filesystem
    init_filesystem();

    // Test 1: Simple embedded Lua script
    const char *simple_script = "answer = 42; print('The answer is: '..answer)";
    run_embedded_lua_test(simple_script, "Simple Embedded Script");

    // Test 2: Run Lua script from a file (fibonacci.lua)
    run_lua_file("fibonacci.lua", "Fibonacci Script from File");

    ESP_LOGI(TAG, "End of Lua example application.");

    // Prevent the task from ending
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
