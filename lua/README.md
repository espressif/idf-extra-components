# Lua - ESP-IDF Component

ESP-IDF component which wraps Lua from upstream repository - https://www.lua.org/

This component provides Lua 5.5.0, a powerful, efficient, lightweight, embeddable scripting language.

## Features

- Lua 5.5.0 (latest version from upstream)
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

This component overrides `luaconf.h` to set `LUA_32BITS` to 1, which is required for ESP32 and other Xtensa/RISC-V architectures.

## License

Lua is licensed under the MIT license. See LICENSE file for details.

## Upstream

- Lua: https://www.lua.org/
- Repository: https://github.com/lua/lua

