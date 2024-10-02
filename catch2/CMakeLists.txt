idf_component_register(SRCS cmd_catch2.cpp
                       INCLUDE_DIRS include)

set(CATCH_CONFIG_NO_POSIX_SIGNALS 1 CACHE BOOL OFF FORCE)
add_subdirectory(Catch2)

target_link_libraries(${COMPONENT_LIB} PUBLIC Catch2::Catch2)
get_target_property(catch_target Catch2::Catch2 ALIASED_TARGET)

# Silence a warning in catch_exception_translator_registry.cpp
target_compile_options(${catch_target} PRIVATE -Wno-unused-function)

# Link to pthreads to avoid issues with STL headers
idf_build_get_property(target IDF_TARGET)
if(NOT target STREQUAL "linux")
    target_link_libraries(${catch_target} PUBLIC idf::pthread)
    # Work around a linking dependency issue in ESP-IDF
    target_link_libraries(${COMPONENT_LIB} PUBLIC "-Wl,-u getentropy")
endif()

# If console component is present in the build, include the console
# command feature.
idf_build_get_property(build_components BUILD_COMPONENTS)
if("console" IN_LIST build_components)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WITH_CONSOLE)
    target_link_libraries(${COMPONENT_LIB} PUBLIC idf::console)
endif()
