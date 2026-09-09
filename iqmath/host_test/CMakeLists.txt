# This is the project CMakeLists.txt file for the host test subproject
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main)
project(iqmath_host_test)
