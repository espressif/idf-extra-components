idf_component_register(
    SRCS
        "lua/lapi.c"
        "lua/lauxlib.c"
        "lua/lbaselib.c"
        "lua/lcode.c"
        "lua/lctype.c"
        "lua/lcorolib.c"
        "lua/ldblib.c"
        "lua/ldebug.c"
        "lua/ldo.c"
        "lua/lfunc.c"
        "lua/lgc.c"
        "lua/linit.c"
        "lua/liolib.c"
        "lua/llex.c"
        "lua/lmathlib.c"
        "lua/lmem.c"
        "lua/lopcodes.c"
        "lua/loadlib.c"
        "lua/loslib.c"
        "lua/lstate.c"
        "lua/ltable.c"
        "lua/ltm.c"
        "lua/lvm.c"
        "lua/lobject.c"
        "lua/lparser.c"
        "lua/lstrlib.c"
        "lua/lstring.c"
        "lua/ltablib.c"
        "lua/ldump.c"
        "lua/lundump.c"
        "lua/lutf8lib.c"
        "lua/lzio.c"
    INCLUDE_DIRS "lua"
    PRIV_REQUIRES heap esp_psram
)

# LUA_32BITS decides the width of lua_Integer/lua_Number and thus the layout of
# every public struct. It has to be PUBLIC: consumers calling lua_tonumber() or
# lua_pushinteger() must be compiled against the very same types the library was
# built with, otherwise the ABI silently diverges.
target_compile_definitions(${COMPONENT_LIB} PUBLIC LUA_32BITS=1)

# LUAI_MAXSTACK is only consumed by ldo.c, so it stays PRIVATE.
target_compile_definitions(${COMPONENT_LIB} PRIVATE LUAI_MAXSTACK=${CONFIG_LUA_MAXSTACK})

# Upstream sources are third-party code, keep their warnings out of our builds
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-function)

if(CONFIG_LUA_USE_PSRAM)
    target_sources(${COMPONENT_LIB} PRIVATE "lua_memory.c")
    # luaL_newstate() calls lua_newstate() across TUs; --wrap intercepts that.
    target_link_libraries(${COMPONENT_LIB} INTERFACE
        "-Wl,--wrap=lua_newstate"
        "-Wl,-u,__wrap_lua_newstate")
endif()
