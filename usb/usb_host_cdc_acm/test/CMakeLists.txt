idf_component_register(SRCS "test_cdc_acm_host.c" "usb_device.c"
                       INCLUDE_DIRS "."
                       REQUIRES usb_host_cdc_acm tinyusb unity)
