idf_component_register(SRCS "test_app_main.c"
                            "test_cbor.c"
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES unity
                       WHOLE_ARCHIVE)
