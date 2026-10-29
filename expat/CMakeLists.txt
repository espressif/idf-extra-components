idf_build_get_property(_expat_idf_target IDF_TARGET)

idf_component_register()

# Package as a static library; skip tools, tests, docs, install.
set(EXPAT_SHARED_LIBS OFF)
set(EXPAT_BUILD_TOOLS OFF)
set(EXPAT_BUILD_EXAMPLES OFF)
set(EXPAT_BUILD_TESTS OFF)
set(EXPAT_BUILD_DOCS OFF)
set(EXPAT_BUILD_PKGCONFIG OFF)
set(EXPAT_ENABLE_INSTALL OFF)
set(EXPAT_ENABLE_EXPORT OFF)

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

# Upstream's feature checks run in a standalone try_compile without IDF
# headers, so seed the results instead of reconstructing that environment.
if(NOT "${_expat_idf_target}" STREQUAL "linux")
    set(EXPAT_WITH_GETRANDOM ON)
    set(HAVE_GETRANDOM TRUE)
endif()
unset(_expat_idf_target)

add_subdirectory(./expat/expat ${CMAKE_CURRENT_BINARY_DIR}/upstream)

# expat.h also reads these; consumers need the prototypes.
target_compile_definitions(expat INTERFACE
    $<$<BOOL:${EXPAT_DTD}>:XML_DTD>
    $<$<BOOL:${EXPAT_GE}>:XML_GE=1>
)

target_link_libraries(${COMPONENT_LIB} INTERFACE expat)
