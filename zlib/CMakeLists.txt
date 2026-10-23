idf_component_register(SRCS "zlib_memory.c"
                       PRIV_REQUIRES heap esp_psram)

# Static only: the shared library carries the version script, while tests,
# contrib and install rules are of no use inside ESP-IDF.
set(ZLIB_BUILD_SHARED OFF)
set(ZLIB_BUILD_STATIC ON)
set(ZLIB_BUILD_TESTING OFF)
set(ZLIB_INSTALL OFF)

add_subdirectory(zlib)

# zutil.c drops its own zcalloc()/zcfree() under this macro, so the ones in
# zlib_memory.c take over every allocation zlib makes for a stream.
target_compile_definitions(zlibstatic PRIVATE MY_ZCALLOC=1)
target_link_libraries(${COMPONENT_LIB} PUBLIC zlibstatic)

# zcalloc()/zcfree() live in zlib_memory.c (this component archive). The only
# references are inside zlibstatic, so without -u the linker drops that object
# before libz.a is scanned.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,-u,zcalloc")
