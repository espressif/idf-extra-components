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

create_esp_enc_img(${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
    ${project_dir}/rsa_key/private.pem ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}_secure.bin app)

if(CONFIG_EXAMPLE_ENABLE_CI_TEST)
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::esp_https_server)
endif()
