set(SRCS "main.c")
set(INCLUDE_DIRS ".")
set(EMBED_TXTFILES "tests/certs/servercert.pem")
set(PRIV_REQS "esp_http_client esp_partition nvs_flash app_update esp_timer esp_wifi console esp_eth esp_https_server")

if(CONFIG_EXAMPLE_ENABLE_CI_TEST)
    list(APPEND SRCS
        "tests/test_local_server_ota.c")
    list(APPEND INCLUDE_DIRS "tests")
    list(APPEND EMBED_TXTFILES "tests/certs/prvtkey.pem")
endif()

idf_component_register(SRCS ${SRCS}
                    INCLUDE_DIRS ${INCLUDE_DIRS}
                    EMBED_TXTFILES ${EMBED_TXTFILES}
                    PRIV_REQUIRES ${PRIV_REQS})
