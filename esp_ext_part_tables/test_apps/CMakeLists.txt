cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main)
project(esp_ext_part_tables_parse_test)

# Force the LittleFS branch of esp_ext_part_match_mountable on for tests, so the
# host test can verify LittleFS-is-mountable without linking the LittleFS component.
idf_build_set_property(COMPILE_DEFINITIONS "ESP_EXT_PART_HAS_LITTLEFS=1" APPEND)
