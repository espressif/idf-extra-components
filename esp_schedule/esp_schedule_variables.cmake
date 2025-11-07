# This file is used to set the common non-glue variables for the esp_schedule component.
# Include this file and extend the variables to add glue sources and include directories.

# Source files
set(ESP_SCHEDULE_SRCS "${CMAKE_CURRENT_LIST_DIR}/src/esp_schedule.c")

# Include directories
set(ESP_SCHEDULE_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include/common")

# Private include directories
set(ESP_SCHEDULE_PRIV_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
                                   "${CMAKE_CURRENT_LIST_DIR}/glue")

# Private requirements
set(ESP_SCHEDULE_PRIV_REQUIRES "esp_daylight")