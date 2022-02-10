# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS ../libsodium ../expat ../esp_encrypted_img)

# Set the components to include the tests for.
set(TEST_COMPONENTS libsodium expat esp_encrypted_img CACHE STRING "List of components to test")

include($ENV{IDF_PATH}/tools/cmake/version.cmake) # $ENV{IDF_VERSION} was added after v4.3...
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "4.4")
    set(EXCLUDE_COMPONENTS esp_encrypted_img)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(idf_extra_test_app)
