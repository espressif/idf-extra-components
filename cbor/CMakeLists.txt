idf_build_get_property(target IDF_TARGET)

idf_component_register()

# tinycbor's option() names are unprefixed: CMP0077 NEW keeps our values
# local instead of leaking them into the project cache.
# TODO: drop once tinycbor's cmake_minimum_required() is >= 3.13.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(WITH_FLOATING_POINT ON)
set(WITH_FREESTANDING OFF)
set(WITH_CBOR2JSON ON)
set(BUILD_EXAMPLES OFF)
set(BUILD_TOOLS OFF)
set(BUILD_TESTING OFF)
set(ENABLE_INSTALL OFF)

# Upstream's feature checks run in a standalone try_compile without IDF
# headers, so seed the results instead of reconstructing that environment.
# Keep them as plain variables so stale cache entries can't survive a
# target switch in the same build dir.
if(NOT target STREQUAL "linux")
    if("${IDF_VERSION_MAJOR}" VERSION_LESS "6")
        # 5.x newlib declares open_memstream() but has no implementation
        set(HAVE_OPEN_MEMSTREAM FALSE)
        set(HAVE_OPEN_FUNOPEN TRUE)
        set(HAVE_OPEN_FUNOPEN_SSIZE FALSE)
    else()
        set(HAVE_OPEN_MEMSTREAM TRUE)
    endif()
endif()

add_subdirectory(tinycbor EXCLUDE_FROM_ALL)

target_link_libraries(${COMPONENT_LIB} INTERFACE tinycbor)
