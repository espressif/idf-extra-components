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

# Upstream's check_symbol_exists() / check_c_source_compiles() run in a
# standalone try_compile: no IDF _GNU_SOURCE, no esp_libc wrapper headers.
# Seed the results rather than trying to reconstruct that environment.
if(NOT ${target} STREQUAL "linux")
    if("${IDF_VERSION_MAJOR}" VERSION_LESS "6")
        # 5.x newlib only declares open_memstream() without implementation
        set(HAVE_OPEN_MEMSTREAM FALSE CACHE INTERNAL "ESP-IDF 5.x newlib has no open_memstream()" FORCE)
        set(HAVE_OPEN_FUNOPEN TRUE CACHE INTERNAL "ESP-IDF 5.x newlib provides funopen()" FORCE)
        set(HAVE_OPEN_FUNOPEN_SSIZE FALSE CACHE INTERNAL "ESP-IDF 5.x funopen is BSD, not picolibc" FORCE)
    else()
        # 6+ ships open_memstream()
        set(HAVE_OPEN_MEMSTREAM TRUE CACHE INTERNAL "ESP-IDF 6+ provides open_memstream()" FORCE)
    endif()
endif()

# tinycbor has no install-off switch. Skip its install() rules so they do not
# write headers/pkgconfig into the IDF prefix, and so install(EXPORT) does not
# see IDF interface libraries inherited through link_libraries().
set(_cbor_skip_install "${CMAKE_SKIP_INSTALL_RULES}")
set(CMAKE_SKIP_INSTALL_RULES ON)
add_subdirectory(tinycbor EXCLUDE_FROM_ALL)
set(CMAKE_SKIP_INSTALL_RULES "${_cbor_skip_install}")
unset(_cbor_skip_install)

target_link_libraries(${COMPONENT_LIB} INTERFACE tinycbor)
