idf_component_register()

# Package as a static library; skip tools, tests, docs, install.
set(EXPAT_SHARED_LIBS OFF)
set(EXPAT_BUILD_TOOLS OFF)
set(EXPAT_BUILD_EXAMPLES OFF)
set(EXPAT_BUILD_TESTS OFF)
set(EXPAT_BUILD_DOCS OFF)
set(EXPAT_BUILD_PKGCONFIG OFF)
set(EXPAT_ENABLE_INSTALL OFF)

# Kconfig → EXPAT_*. Always ON/OFF; an unset CONFIG_* unsets the var
# and shy_set puts the default/cached ON back (DTD without GE is an error).
if(CONFIG_EXPAT_XML_DTD)
    set(EXPAT_DTD ON)
else()
    set(EXPAT_DTD OFF)
endif()
if(CONFIG_EXPAT_XML_GE)
    set(EXPAT_GE ON)
else()
    set(EXPAT_GE OFF)
endif()

# Chip: try_compile cannot see esp_libc. UNIX is false on IDF, so also set
# EXPAT_WITH_GETRANDOM or evaluate_detection_results() zeroes the seed.
idf_build_get_property(_expat_idf_target IDF_TARGET)
if(NOT "${_expat_idf_target}" STREQUAL "linux")
    set(EXPAT_WITH_GETRANDOM ON)
    set(HAVE_GETRANDOM TRUE CACHE INTERNAL "ESP-IDF provides getrandom() in esp_libc" FORCE)
endif()
unset(_expat_idf_target)

# idf_component_register() calls link_libraries() for IDF's common
# requirements. add_subdirectory() inherits those, so the upstream
# target picks up __idf_* (headers and libs, e.g. esp_libc). That is
# what we want. CMake then rejects export(TARGETS ...) because those
# libraries are not in an export set.
#
# IDF never loads the generated build-tree *-targets.cmake, so export()
# is defined empty on purpose.
#
# The body stays empty: do not call _export(). A second component may
# define the same empty export(); CMake would then alias _export to
# this no-op, and forwarding would recurse.
function(export)
endfunction()

add_subdirectory(./expat/expat ${CMAKE_CURRENT_BINARY_DIR}/upstream)

# expat.h also reads these; consumers need the prototypes.
target_compile_definitions(expat INTERFACE
    $<$<BOOL:${EXPAT_DTD}>:XML_DTD>
    $<$<BOOL:${EXPAT_GE}>:XML_GE=1>
)

target_link_libraries(${COMPONENT_LIB} INTERFACE expat)
