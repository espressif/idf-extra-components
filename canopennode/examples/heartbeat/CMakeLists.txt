# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

project(canopen_heartbeat)

# Add the OD.h and OD.c files path for canopennode component
idf_component_get_property(canopennode_lib canopennode COMPONENT_LIB)
target_include_directories(${canopennode_lib} PRIVATE ${CMAKE_SOURCE_DIR}/main)
