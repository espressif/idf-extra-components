idf_build_get_property(target IDF_TARGET)

idf_component_register()

# tinycbor's option() names are unprefixed. Honor local values without writing
# them into the project cache (CMP0077); cmake_minimum_required(3.10) in the
# subdirectory would otherwise ignore these and keep the cache.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(WITH_FLOATING_POINT ON)
set(WITH_FREESTANDING OFF)
set(WITH_CBOR2JSON ON)
set(BUILD_EXAMPLES OFF)
set(BUILD_TOOLS OFF)
set(BUILD_TESTING OFF)

# check_symbol_exists() cannot see that ESP-IDF 5.x newlib only declares
# open_memstream(). Seed the probe so upstream selects the funopen() fallback.
if(NOT ${target} STREQUAL "linux" AND "${IDF_VERSION_MAJOR}" VERSION_LESS "6")
    set(HAVE_OPEN_MEMSTREAM FALSE CACHE INTERNAL "ESP-IDF 5.x newlib has no open_memstream()")
    set(HAVE_OPEN_FUNOPEN TRUE CACHE INTERNAL "ESP-IDF 5.x newlib provides funopen()")
endif()

# tinycbor has no install-off switch. Skip its install() rules so they do not
# write headers/pkgconfig into the IDF prefix, and so install(EXPORT) does not
# see IDF interface libraries inherited through link_libraries().
set(_cbor_skip_install "${CMAKE_SKIP_INSTALL_RULES}")
set(CMAKE_SKIP_INSTALL_RULES ON)
add_subdirectory(tinycbor)
set(CMAKE_SKIP_INSTALL_RULES "${_cbor_skip_install}")
unset(_cbor_skip_install)

# Host gcc with -std=gnu23 gives strchr() the const of its argument.
# Upstream stores the result in a char * in cbortojson.c
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/tinycbor/src/cbortojson.c"
                            TARGET_DIRECTORY tinycbor
                            PROPERTIES COMPILE_OPTIONS -Wno-error=discarded-qualifiers)

target_link_libraries(${COMPONENT_LIB} INTERFACE tinycbor)
