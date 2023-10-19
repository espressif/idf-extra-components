# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
include($ENV{IDF_PATH}/tools/cmake/version.cmake)

# Add newly added components to one of these lines:
# 1. Add here if the component is compatible with IDF >= v4.3
set(EXTRA_COMPONENT_DIRS ../libsodium ../expat ../cbor ../jsmn ../qrcode ../coap ../eigen ../fmt ../json_parser ../json_generator ../esp_delta_ota ../quirc)

# 2. Add here if the component is compatible with IDF >= v4.4
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "4.4")
    list(APPEND EXTRA_COMPONENT_DIRS ../iqmath ../pid_ctrl ../esp_encrypted_img ../pcap ../esp_jpeg ../libpng ../zlib  ../coremark ../freetype)
endif()

# 3. Add here if the component is compatible with IDF >= v5.0
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
    list(APPEND EXTRA_COMPONENT_DIRS ../bdc_motor ../led_strip ../sh2lib ../nghttp ../esp_serial_slave_link ../onewire_bus ../ccomp_timer ../catch2 ../dhara)
endif()

# 4. Add here if the component is compatible with IDF >= v5.1
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.1")
    list(APPEND EXTRA_COMPONENT_DIRS ../network_provisioning)
endif()

# 5. Add here if the component is compatible with IDF >= v5.3
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    list(APPEND EXTRA_COMPONENT_DIRS ../esp_lcd_qemu_rgb)
endif()

# !This section should NOT be touched when adding new component!
# Take all components in EXTRA_COMPONENT_DIRS, strip leading '../' and add it to TEST_COMPONENTS
# The build system will build and link unit tests, if the component contains 'test' subdirectory
set(TEST_COMPONENTS "" CACHE STRING "List of components to test")
foreach (CMP_DIR ${EXTRA_COMPONENT_DIRS})
    string(SUBSTRING ${CMP_DIR} 3 100 STRIPPED_CMP) # There should be no component name longer than 100 bytes...
    list(APPEND TEST_COMPONENTS ${STRIPPED_CMP})
endforeach()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(idf_extra_test_app)
