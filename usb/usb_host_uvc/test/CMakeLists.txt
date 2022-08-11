idf_component_register(SRCS "test_uvc.c" "libusb_parse.c"
                       INCLUDE_DIRS "." "../private_include"
                       REQUIRES usb_host_uvc usb unity)
