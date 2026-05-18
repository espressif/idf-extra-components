/*
** Lua port configuration for ESP-IDF
**
** This file provides platform-specific overrides for Lua configuration.
** It uses header injection technique to modify upstream Lua behavior
** without forking the entire luaconf.h file.
**
** The upstream luaconf.h is included first, then specific macros are
** overridden for ESP-IDF platform requirements.
*/

#ifndef LUA_PORT_LUACONF_H
#define LUA_PORT_LUACONF_H

#ifdef __IDF__
/*
** Include upstream luaconf.h to get all default definitions.
** This ensures we have access to all upstream configuration.
*/
#include "luaconf.h"

/*
** ESP-IDF specific overrides
*/

/*
** LUA_32BITS enables Lua with 32-bit integers and 32-bit floats.
** This is required for ESP32 and other Xtensa/RISC-V architectures.
*/
#undef LUA_32BITS
#define LUA_32BITS 1

/*
** LUAI_MAXSTACK limits the size of the Lua stack.
** Override to use ESP-IDF Kconfig value (CONFIG_LUA_MAXSTACK).
** This allows users to configure stack size via menuconfig.
*/
#if 1000000 < (INT_MAX / 2)
#undef LUAI_MAXSTACK
#define LUAI_MAXSTACK       CONFIG_LUA_MAXSTACK
#else
#undef LUAI_MAXSTACK
#define LUAI_MAXSTACK       (INT_MAX / 2u)
#endif

#endif /* __IDF__ */

#endif /* LUA_PORT_LUACONF_H */
