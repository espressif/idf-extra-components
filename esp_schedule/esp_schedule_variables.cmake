# Platform-independent half of the esp_schedule component.
#
# Everything listed here reaches the outside world only through the function
# pointers in include/esp_schedule.h, so it builds anywhere. Include this
# file and append your own port sources to build esp_schedule against a
# platform other than ESP-IDF; see CMakeLists.txt for how the ESP-IDF port does
# exactly that.
#
# Note that src/esp_schedule_default.c is deliberately NOT listed here. It is
# the only referrer of the ESP-IDF port tables, and keeping it in a separate
# object is what lets the linker drop the default port for anyone who calls
# esp_schedule_init_with_config() instead of esp_schedule_init().

# Source files
set(ESP_SCHEDULE_SRCS "${CMAKE_CURRENT_LIST_DIR}/src/esp_schedule.c"
                      "${CMAKE_CURRENT_LIST_DIR}/src/esp_schedule_nvs.c"
                      "${CMAKE_CURRENT_LIST_DIR}/src/esp_schedule_port.c")

# Include directories
set(ESP_SCHEDULE_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")

# Private include directories
set(ESP_SCHEDULE_PRIV_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src")

# Private requirements
set(ESP_SCHEDULE_PRIV_REQUIRES "esp_daylight")
