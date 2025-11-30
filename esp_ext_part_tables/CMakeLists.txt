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
