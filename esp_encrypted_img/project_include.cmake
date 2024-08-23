set(ESP_IMG_GEN_TOOL_PATH ${CMAKE_CURRENT_LIST_DIR}/tools/esp_enc_img_gen.py)

idf_build_get_property(build_dir BUILD_DIR)
if(CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES)
    set(app_dependency "${build_dir}/.signed_bin_timestamp")
else()
    set(app_dependency "${build_dir}/.bin_timestamp")
endif()

function(create_esp_enc_img input_file rsa_key_file output_file app)
    cmake_parse_arguments(arg "${options}" "" "${multi}" "${ARGN}")
    idf_build_get_property(python PYTHON)

    add_custom_command(OUTPUT ${output_file}
        COMMAND ${python} ${ESP_IMG_GEN_TOOL_PATH} encrypt
            ${input_file}
            ${rsa_key_file} ${output_file}
        DEPENDS "${app_dependency}"
        COMMENT "Generating pre-encrypted binary"
        VERBATIM
    )
    get_filename_component(name ${output_file} NAME_WE)
    add_custom_target(enc_bin_target_${name} ALL DEPENDS ${output_file})
    add_dependencies(enc_bin_target_${name} gen_project_binary)
endfunction()
