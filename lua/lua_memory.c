/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_heap_caps.h"
#include "lauxlib.h"
#include "lua.h"

/*
 * Table arrays, long strings, and a grown Lua stack cross this size.
 * Smaller GC objects stay in internal RAM: the VM walks them randomly.
 */
#define LUA_PSRAM_MIN_SIZE (4 * 1024)

static void *lua_psram_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    (void)ud;
    (void)osize; /* lua_newstate passes a type tag here when ptr is NULL */

    if (nsize == 0) {
        heap_caps_free(ptr);
        return NULL;
    }
    if (nsize >= LUA_PSRAM_MIN_SIZE) {
        return heap_caps_realloc_prefer(ptr, nsize, 2,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return heap_caps_realloc_prefer(ptr, nsize, 2,
                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

lua_State *__real_lua_newstate(lua_Alloc f, void *ud, unsigned seed);

/*
 * luaL_newstate() lives in the same translation unit as luaL_alloc, so
 * wrapping luaL_alloc would miss it. lua_newstate() is in lstate.c;
 * intercept that and swap in the caps allocator when the caller asked
 * for the libc default.
 */
lua_State *__wrap_lua_newstate(lua_Alloc f, void *ud, unsigned seed)
{
    if (f == luaL_alloc) {
        f = lua_psram_alloc;
        ud = NULL;
    }
    return __real_lua_newstate(f, ud, seed);
}
