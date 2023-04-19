idf_component_register(SRCS "src/onewire_bus_api.c"
                            "src/onewire_bus_impl_rmt.c"
                            "src/onewire_crc.c"
                            "src/onewire_device.c"
                       INCLUDE_DIRS "include" "interface"
                       PRIV_REQUIRES driver)
