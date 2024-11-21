idf_build_get_property(project_dir PROJECT_DIR)

if(CONFIG_EXAMPLE_ENABLE_CI_TEST)
    list(APPEND SRCS
        "test_dir/test_local_server_ota.c")
    list(APPEND INCLUDE_DIRS "test_dir")
    list(APPEND EMBED_TXTFILES "test_dir/certs/servercert.pem"
                               "test_dir/certs/prvtkey.pem")
endif()

idf_component_register(SRCS "pre_encrypted_ota.c" ${SRCS}
                    PRIV_REQUIRES esp_http_client app_update esp_https_ota nvs_flash esp_netif esp_wifi esp_netif esp_partition
                    INCLUDE_DIRS "." ${INCLUDE_DIRS}
                    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem
                                   ${project_dir}/rsa_key/private.pem
                                   ${EMBED_TXTFILES})

create_esp_enc_img(${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
    ${project_dir}/rsa_key/private.pem ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}_secure.bin app)

if(CONFIG_EXAMPLE_ENABLE_CI_TEST)
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::esp_https_server)
endif()
