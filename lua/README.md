# Lua for ESP-IDF

[![Component Registry](https://components.espressif.com/components/espressif/lua/badge.svg)](https://components.espressif.com/components/espressif/lua)

This component packages [Lua](https://www.lua.org/) for ESP-IDF. Include the upstream headers and call the upstream API.

## Add the component

From an ESP-IDF project:

```bash
idf.py add-dependency "espressif/lua^5.5.1"
```

Alternatively, add this to `main/idf_component.yml`:

```yaml
dependencies:
  espressif/lua: "^5.5.1"
```

## Use it in a project

```c
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

void run_lua(void)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dostring(L, "print('Hello from Lua!')") != LUA_OK) {
        printf("%s\n", lua_tostring(L, -1));
    }

    lua_close(L);
}
```

See [examples/lua_example](examples/lua_example) for a complete ESP-IDF project, including loading a script from the filesystem.

## On ESP-IDF

These are the only settings this component changes relative to a stock Lua build:

- **32-bit numbers.** `lua_Integer` is 32-bit and `lua_Number` is `float` (about 7 significant decimal digits). Large integers pushed and read back through `lua_tonumber()` can lose precision.
- **`CONFIG_LUA_MAXSTACK`.** Menuconfig option **Lua → Maximum Lua stack size (slots)**. This is a growth cap on Lua values, not a reservation and not a byte count. The stack starts small and expands on demand. Size it together with the calling FreeRTOS task stack.
- **`CONFIG_LUA_USE_PSRAM`.** Off by default; needs PSRAM. `luaL_newstate()` then puts allocations of 4 KiB or more in PSRAM first (table arrays, long strings, a large Lua stack) and keeps smaller objects in internal RAM. A custom allocator passed to `lua_newstate()` is not changed.

There is no ESP-specific wrapper API.

## Programming guide

For the language, C API, standard libraries, and embedding rules, use the official Lua documentation:

- [Lua reference manual](https://www.lua.org/manual/)

## License

Lua is licensed under the MIT license. See [LICENSE](LICENSE) for details.
