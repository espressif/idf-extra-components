/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Implementation of the C modules exposed to the Lua scripts.
 *
 * Registration is split by topic and left to the application: main() creates
 * the state, then calls app_lua_open_libs() and one register function per topic.
 *
 * C modules go into package.preload, not the global table. Scripts load them
 * with require(), the same way they load a .lua file from the filesystem:
 *
 *   local esp = require("esp")
 *   local led_strip = require("led_strip")
 *
 *   esp.free_heap()                      free heap in bytes
 *   esp.sleep_ms(ms)                     let the firmware task breathe
 *   led_strip.led_count                  number of pixels (integer field)
 *   led_strip.set_pixel(i, r, g, b)      set one pixel of the strip
 *   led_strip.refresh()                  flush the pixel buffer to the strip
 *
 * Bad arguments (wrong type, out of range) raise a Lua error. Hardware
 * failures return false plus an esp_err name, so a script can recover.
 *
 * Memory comes from the component's own allocator: enable
 * CONFIG_LUA_USE_PSRAM to let large Lua objects live in PSRAM, and tune
 * CONFIG_LUA_MAXSTACK for the value stack. This file deliberately does not
 * install its own lua_Alloc, so it does not duplicate what the component
 * already does.
 */

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "led_strip.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "app_lua.h"

#define TAG "app_lua"

// GPIO the LED strip data line is connected to
#define LED_STRIP_GPIO_PIN 2
// Number of LEDs in the strip
#define LED_STRIP_LED_COUNT 24
// 10MHz resolution, 1 tick = 0.1us (LED strips need a high resolution)
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)

/*
 * Load only the libraries this example needs. Prefer luaL_requiref() over
 * luaL_openlibs(): that keeps a table of every luaopen_* in linit.c, so
 * --gc-sections cannot drop the libraries we never call.
 *
 * package is required so that scripts can call require(). C modules in this
 * example are published through package.preload.
 */
void app_lua_open_libs(lua_State *L)
{
    static const luaL_Reg libs[] = {
        {LUA_GNAME, luaopen_base},
        {LUA_LOADLIBNAME, luaopen_package},
        {LUA_MATHLIBNAME, luaopen_math},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_UTF8LIBNAME, luaopen_utf8},
        {NULL, NULL},
    };

    for (const luaL_Reg *lib = libs; lib->func != NULL; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }
}

void app_lua_set_search_path(lua_State *L, const char *dir)
{
    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        ESP_LOGE(TAG, "package library is not open; call app_lua_open_libs() first");
        return;
    }
    lua_pushfstring(L, "%s/?.lua;%s/?/init.lua", dir, dir);
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);
}

/* Put the opener on top of the stack into package.preload[name]. */
static void preload_module(lua_State *L, const char *name)
{
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    lua_insert(L, -2);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

// Create the LED strip used by the Lua script
void app_lua_init_led_strip(led_strip_handle_t *led_strip)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_PIN,
        .max_leds = LED_STRIP_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .mem_block_symbols = 0, /* let the driver pick a proper block size */
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
}

void app_lua_deinit_led_strip(led_strip_handle_t led_strip)
{
    if (led_strip != NULL) {
        led_strip_del(led_strip);
    }
}

/* esp.free_heap(): free heap in bytes. Usage: esp.free_heap() / 1024 */
static int l_esp_free_heap(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    return 1; /* number of values pushed onto the stack */
}

/* esp.sleep_ms(ms): let the firmware task breathe between script steps. */
static int l_esp_sleep_ms(lua_State *L)
{
    lua_Integer ms = luaL_checkinteger(L, 1);
    luaL_argcheck(L, ms >= 0, 1, "milliseconds must not be negative");
    vTaskDelay(pdMS_TO_TICKS((uint32_t)ms));
    return 0;
}

static int luaopen_esp(lua_State *L)
{
    static const luaL_Reg funcs[] = {
        {"free_heap", l_esp_free_heap},
        {"sleep_ms", l_esp_sleep_ms},
        {NULL, NULL},
    };
    luaL_newlib(L, funcs);
    return 1;
}

void app_lua_register_esp(lua_State *L)
{
    lua_pushcfunction(L, luaopen_esp);
    preload_module(L, "esp");
}

static uint32_t check_u8(lua_State *L, int arg)
{
    lua_Integer value = luaL_checkinteger(L, arg);
    luaL_argcheck(L, value >= 0 && value <= 255, arg, "expected 0..255");
    return (uint32_t)value;
}

/* led_strip.set_pixel(index, r, g, b): the LED strip handle is captured. */
static int l_led_set_pixel(lua_State *L)
{
    led_strip_handle_t led_strip = (led_strip_handle_t)lua_touserdata(L, lua_upvalueindex(1));
    lua_Integer index = luaL_checkinteger(L, 1);
    if (index < 0 || index >= LED_STRIP_LED_COUNT) {
        return luaL_error(L, "bad argument #1 to 'set_pixel' (LED index out of range, expected 0..%d)",
                          LED_STRIP_LED_COUNT - 1);
    }
    uint32_t red = check_u8(L, 2);
    uint32_t green = check_u8(L, 3);
    uint32_t blue = check_u8(L, 4);

    esp_err_t err = led_strip_set_pixel(led_strip, (uint32_t)index, red, green, blue);
    if (err != ESP_OK) {
        /* Return the error instead of raising, so a script can react to it. */
        lua_pushboolean(L, false);
        lua_pushstring(L, esp_err_to_name(err));
        return 2;
    }
    lua_pushboolean(L, true);
    return 1;
}

/* led_strip.refresh(): flush the pixel buffer to the strip. */
static int l_led_refresh(lua_State *L)
{
    led_strip_handle_t led_strip = (led_strip_handle_t)lua_touserdata(L, lua_upvalueindex(1));
    esp_err_t err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        lua_pushboolean(L, false);
        lua_pushstring(L, esp_err_to_name(err));
        return 2;
    }
    lua_pushboolean(L, true);
    return 1;
}

/* The handle is the opener's upvalue; copy it onto every method. */
static int luaopen_led_strip(lua_State *L)
{
    static const luaL_Reg funcs[] = {
        {"set_pixel", l_led_set_pixel},
        {"refresh", l_led_refresh},
        {NULL, NULL},
    };
    lua_newtable(L);
    lua_pushvalue(L, lua_upvalueindex(1));
    luaL_setfuncs(L, funcs, 1);
    lua_pushinteger(L, LED_STRIP_LED_COUNT);
    lua_setfield(L, -2, "led_count");
    return 1;
}

void app_lua_register_led_strip(lua_State *L, led_strip_handle_t led_strip)
{
    lua_pushlightuserdata(L, led_strip);
    lua_pushcclosure(L, luaopen_led_strip, 1);
    preload_module(L, "led_strip");
}
