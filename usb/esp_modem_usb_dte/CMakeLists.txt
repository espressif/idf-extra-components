idf_component_register(SRCS "esp_modem_usb.cpp" "esp_modem_usb_api_target.cpp" "esp_modem_usb_c_api.cpp"
                       REQUIRED_IDF_TARGETS esp32s2 esp32s3
                       PRIV_INCLUDE_DIRS "private_include"
                       INCLUDE_DIRS "include")

set_target_properties(${COMPONENT_LIB} PROPERTIES CXX_STANDARD 17)

# Determine whether esp_modem is fetched from component registry or from local path
idf_build_get_property(build_components BUILD_COMPONENTS)
if(esp_modem IN_LIST build_components)
    set(esp_modem_name esp_modem) # Local component
else()
    set(esp_modem_name espressif__esp_modem) # Managed component
endif()

# esp_modem_usb_dte includes some files from private includes of esp_modem
idf_component_get_property(esp_modem_dir ${esp_modem_name} COMPONENT_DIR)
idf_component_get_property(esp_modem_include ${esp_modem_name} INCLUDE_DIRS)
target_include_directories(${COMPONENT_LIB} PRIVATE "${esp_modem_dir}/${esp_modem_include}")
