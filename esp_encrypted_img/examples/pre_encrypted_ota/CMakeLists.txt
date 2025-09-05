# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pre_encrypted_ota)

# Flash the pre_encrypted_ota_secure.bin to the OTA 1 partition.
if(CONFIG_EXAMPLE_ENABLE_CI_TEST)
    set(partition ota_1)
    idf_build_get_property(build_dir BUILD_DIR)
    set(image_file ${build_dir}/pre_encrypted_ota_secure.bin)
    partition_table_get_partition_info(offset "--partition-name ${partition}" "offset")
    esptool_py_flash_target_image(flash "${partition}" "${offset}" "${image_file}")
endif()

if (CONFIG_PRE_ENCRYPTED_RSA_USE_DS)
    set(partition esp_secure_cert)
    idf_build_get_property(project_dir PROJECT_DIR)
    set(image_file ${CMAKE_CURRENT_SOURCE_DIR}/esp_secure_cert_data/${partition}.bin)
    partition_table_get_partition_info(offset "--partition-name ${partition}" "offset")
    esptool_py_flash_target_image(flash "${partition}" "${offset}" "${image_file}")
endif()
