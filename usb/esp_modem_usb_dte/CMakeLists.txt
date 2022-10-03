idf_component_register(SRCS "esp_modem_usb.cpp" "esp_modem_usb_api_target.cpp" "esp_modem_usb_c_api.cpp"
                       REQUIRED_IDF_TARGETS esp32s2 esp32s3
                       PRIV_INCLUDE_DIRS "private_include"
                       INCLUDE_DIRS "include")

set_target_properties(${COMPONENT_LIB} PROPERTIES CXX_STANDARD 17)
