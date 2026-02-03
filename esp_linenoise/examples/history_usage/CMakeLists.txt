cmake_minimum_required(VERSION 3.22)

# Use custom partition table for SPIFFS storage (ESP32 targets only)
if(NOT "$ENV{IDF_TARGET}" STREQUAL "linux")
    set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/components/spiffs)
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main)
project(history_usage)