set(ESP_IMG_GEN_TOOL_PATH ${CMAKE_CURRENT_LIST_DIR}/tools/esp_enc_img_gen.py)
set(ESP_IMG_GEN_SECURE_CERT_DATA_TOOL_PATH 
    ${CMAKE_SOURCE_DIR}/managed_components/espressif__esp_secure_cert_mgr/tools/configure_esp_secure_cert.py)

idf_build_get_property(build_dir BUILD_DIR)
if(CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES)
    set(app_dependency "${build_dir}/.signed_bin_timestamp")
else()
    set(app_dependency "${build_dir}/.bin_timestamp")
endif()

function(create_esp_enc_img input_file key_file output_file app)
    cmake_parse_arguments(arg "${options}" "" "${multi}" "${ARGN}")
    idf_build_get_property(python PYTHON)

    add_custom_command(OUTPUT ${output_file}
        COMMAND ${python} ${ESP_IMG_GEN_TOOL_PATH} encrypt 
            ${input_file} ${key_file}
            ${output_file}
        DEPENDS "${app_dependency}"
        COMMENT "Generating pre-encrypted binary"
        VERBATIM
    )
    get_filename_component(name ${output_file} NAME_WE)
    add_custom_target(enc_bin_target_${name} ALL DEPENDS ${output_file})
    add_dependencies(enc_bin_target_${name} gen_project_binary)
endfunction()

# function(create_esp_enc_img_secure_cert_data target_chip input_key input_key_algo)
#     cmake_parse_arguments(arg "${options}" "" "${multi}" "${ARGN}")
#     idf_build_get_property(python PYTHON)
#     separate_arguments(input_key_algo_args UNIX_COMMAND "${input_key_algo}")
#     add_custom_command(OUTPUT ${input_key}
#         COMMAND ${python} ${ESP_IMG_GEN_SECURE_CERT_DATA_TOOL_PATH}
#             --skip_flash
#             --target_chip ${target_chip}
#             --private-key ${input_key}
#             --priv_key_algo ${input_key_algo_args}
#             -p /dev/cu.usbserial-1230 # This needs to be removed when the esp_secure_cert_mgr tool is updated to not require a port
#         COMMENT "Generating secure data for ${target_chip}"
#         VERBATIM
#     )
#     get_filename_component(name ${input_key} NAME_WE)
#     add_custom_target(enc_secure_cert_data_target_${name} ALL
#         DEPENDS ${input_key}
#     )
#     add_dependencies(enc_secure_cert_data_target_${name} gen_project_binary)
# endfunction()
