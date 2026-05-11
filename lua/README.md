# Lua - ESP-IDF Component

ESP-IDF component which wraps Lua from upstream repository - https://www.lua.org/

This component provides Lua, a powerful, efficient, lightweight, embeddable scripting language.

## Features

- Suitable for embedding Lua code inside ESP-IDF applications
- Configurable stack size limit
- MIT license

## Configuration option

- `LUA_MAXSTACK` - default value: 1000000 - Limits the size of the Lua stack.

Modify values:

```shell
idf.py menuconfig
```

## Usage

This component is suitable for embedding Lua scripts in ESP-IDF applications. For a complete example, see the `examples/lua_example` directory.

### Basic Usage

```c
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

void run_lua() {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    // Run a simple Lua script
    if (luaL_dostring(L, "print('Hello from Lua!')") != LUA_OK) {
        printf("Error: %s\n", lua_tostring(L, -1));
    }

    lua_close(L);
}
```

## Changes to upstream

This component uses header injection to provide ESP-IDF specific configuration overrides without modifying the upstream Lua source code. The platform-specific settings (such as `LUA_32BITS` for ESP32 and Xtensa/RISC-V architectures) are defined in `lua/port/include/luaconf.h`.

## License

Lua is licensed under the MIT license. See LICENSE file for details.

## Upstream

- Lua: https://www.lua.org/
- Repository: https://github.com/lua/lua

