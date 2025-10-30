idf_build_get_property(idf_target IDF_TARGET)

set(srcs "src/esp_commands.c"
         "src/esp_dynamic_commands.c"
         "src/esp_commands_helpers.c")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS private_include
    LDFRAGMENTS linker.lf
    WHOLE_ARCHIVE)

if(${idf_target} STREQUAL "linux")
    # Add custom ld file 
    target_link_options(${COMPONENT_TARGET} INTERFACE "-Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/linux/esp_commands.ld")
endif()
