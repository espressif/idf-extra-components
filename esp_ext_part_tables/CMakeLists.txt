set(srcs "src/esp_ext_part_tables.c"
         "src/esp_mbr.c"
         "src/esp_mbr_utils.c")

set(requires "log" "esp_common")

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.0")
    list(APPEND requires "esp_blockdev")
endif()

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "include"
                    REQUIRES ${requires})

# Auto-enable the LittleFS branch of esp_ext_part_match_mountable() when the LittleFS
# component is part of the build. This component intentionally does not depend on
# LittleFS, so its header is not on our include path and the __has_include() probe in
# esp_mbr_utils.c cannot see it; detect the component here instead. Consumers can
# still force it by defining ESP_EXT_PART_HAS_LITTLEFS themselves.
idf_build_get_property(build_components BUILD_COMPONENTS)
if("joltwallet__littlefs" IN_LIST build_components OR "littlefs" IN_LIST build_components)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ESP_EXT_PART_HAS_LITTLEFS=1)
endif()
