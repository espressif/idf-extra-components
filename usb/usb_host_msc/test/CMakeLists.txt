include($ENV{IDF_PATH}/tools/cmake/version.cmake)
set (TINYUSB_LIB)
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
    set(TINYUSB_LIB "esp_tinyusb")
else()
    set(TINYUSB_LIB "tinyusb")
endif()
idf_component_register(SRC_DIRS .
                       INCLUDE_DIRS .
                       REQUIRES unity usb usb_host_msc ${TINYUSB_LIB})
