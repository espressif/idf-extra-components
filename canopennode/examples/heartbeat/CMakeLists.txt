# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

# Add object dictionary path where the OD.h and OD.c files are located
idf_build_set_property(INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/main" APPEND)

project(canopen_heartbeat)
