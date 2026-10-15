/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Application C modules exposed to the Lua scripts.
 *
 * Everything the firmware publishes to Lua lives here: the hardware handles
 * and the C functions. The application owns the lua_State and decides what to
 * preload, by calling the register functions below after luaL_newstate().
 * Scripts then load those modules with require(), the same way they load a
 * .lua file from the filesystem.
 */

#pragma once

#include "lua.h"
#include "led_strip.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Create the LED strip that the scripts drive. */
void app_lua_init_led_strip(led_strip_handle_t *led_strip);

/* Release the LED strip. Safe to call with NULL. */
void app_lua_deinit_led_strip(led_strip_handle_t led_strip);

/* Open the standard libraries this example needs, including package/require. */
void app_lua_open_libs(lua_State *L);

/* Point package.path at 'dir' so require("name") loads dir/name.lua. */
void app_lua_set_search_path(lua_State *L, const char *dir);

/* Publish the generic IDF helpers as the 'esp' module. */
void app_lua_register_esp(lua_State *L);

/* Publish the LED strip operations as the 'led_strip' module. */
void app_lua_register_led_strip(lua_State *L, led_strip_handle_t led_strip);

#ifdef __cplusplus
}
#endif
