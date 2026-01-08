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
    INCLUDE_DIRS
        "include"
        "lua"
)

# Suppress compiler warnings from upstream Lua code
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-function)

