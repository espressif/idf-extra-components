# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS ../usb_host_cdc_acm
                         ../usb_host_msc
                         ../usb_host_uvc
                         ../esp_modem_usb_dte
                         )

# Set the components to include the tests for.
set(TEST_COMPONENTS "usb_host_cdc_acm" "usb_host_msc" "usb_host_uvc" CACHE STRING "List of components to test")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(usb_test_app)
