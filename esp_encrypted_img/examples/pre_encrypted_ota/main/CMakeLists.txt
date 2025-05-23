idf_build_get_property(project_dir PROJECT_DIR)

set(SRCS "")
set(INCLUDE_DIRS "")
set(EMBED_TXTFILES "")

if(CONFIG_EXAMPLE_ENABLE_CI_TEST)
    list(APPEND SRCS
        "tests/test_local_server_ota.c")
    list(APPEND INCLUDE_DIRS "tests")
    list(APPEND EMBED_TXTFILES "tests/certs/servercert.pem"
                               "tests/certs/prvtkey.pem")
endif()

idf_component_register(SRCS "pre_encrypted_ota.c" ${SRCS}
                    PRIV_REQUIRES esp_http_client app_update esp_https_ota nvs_flash esp_netif esp_wifi esp_netif esp_partition mbedtls
                    INCLUDE_DIRS "." ${INCLUDE_DIRS}
                    EMBED_TXTFILES ${project_dir}/rsa_key/private.pem
                                   ${project_dir}/server_certs/ca_cert.pem
                                   ${EMBED_TXTFILES})

if(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
    set(KEY_FILE "${project_dir}/rsa_key/private.pem")
elseif(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
    set(KEY_FILE "${project_dir}/ecc_key/public.pem")
else()
    message(FATAL_ERROR "Unsupported encryption scheme")
endif()

create_esp_enc_img(${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
    ${KEY_FILE} ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}_secure.bin app)

if(CONFIG_EXAMPLE_ENABLE_CI_TEST)
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::esp_https_server)
endif()

if(CONFIG_PRE_ENCRYPTED_RSA_USE_DS)
    set(TARGET_CHIP $ENV{IDF_TARGET})
    # This is skipped unless esp_secure_cert_mgr
    # can generate secure partition without providing port
    # create_esp_enc_img_secure_cert_data(${TARGET_CHIP} ${KEY_FILE} "RSA 3072")
    idf_component_optional_requires(PRIVATE espressif__esp_secure_cert_mgr)
endif()
