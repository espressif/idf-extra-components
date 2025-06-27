set(srcs "src/onewire_bus_api.c"
         "src/onewire_crc.c"
         "src/onewire_device.c")

if(CONFIG_SOC_RMT_SUPPORTED)
     list(APPEND srcs "src/onewire_bus_impl_rmt.c")
endif()

set(priv_requires)
# Starting from esp-idf v5.3, the RMT drivers are moved to separate components
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    list(APPEND priv_requires "esp_driver_rmt" "esp_driver_gpio")
else()
    list(APPEND priv_requires "driver")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include" "interface"
                       PRIV_REQUIRES ${priv_requires})
