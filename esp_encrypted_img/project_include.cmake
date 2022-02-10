set(ESP_IMG_GEN_TOOL_PATH ${CMAKE_CURRENT_LIST_DIR}/tools/esp_enc_img_gen.py)

function(create_esp_enc_img input_file rsa_key_file output_file app)
    cmake_parse_arguments(arg "${options}" "" "${multi}" "${ARGN}")
    idf_build_get_property(python PYTHON)

    add_custom_command(OUTPUT ${output_file}
        POST_BUILD
        COMMAND ${python} ${ESP_IMG_GEN_TOOL_PATH} encrypt
            ${input_file}
            ${rsa_key_file} ${output_file}
        DEPENDS gen_project_binary
        COMMENT "Generating pre-encrypted binary"
        VERBATIM
    )
    add_custom_target(encrypt_bin_target DEPENDS ${output_file})
    add_dependencies(${app} encrypt_bin_target)
endfunction()
