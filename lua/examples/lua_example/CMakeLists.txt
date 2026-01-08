cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lua_example)

# Create LittleFS partition image
idf_component_register(SRCS "" INCLUDE_DIRS ".")
littlefs_create_partition_image(assets assets FLASH_IN_PROJECT)
