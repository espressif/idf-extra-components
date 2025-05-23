cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main)
project(esp_encrypted_img_test)

if (CONFIG_PRE_ENCRYPTED_RSA_USE_DS)
    set(partition esp_secure_cert)
    idf_build_get_property(project_dir PROJECT_DIR)
    set(image_file ${project_dir}/esp_secure_cert_data/${partition}.bin)
    partition_table_get_partition_info(offset "--partition-name ${partition}" "offset")
    esptool_py_flash_target_image(flash "${partition}" "${offset}" "${image_file}")
endif()
