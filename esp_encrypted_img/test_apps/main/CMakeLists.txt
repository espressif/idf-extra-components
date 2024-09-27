idf_component_register(SRCS "esp_encrypted_img_test.c" "test.c"
                    PRIV_INCLUDE_DIRS "."
                    PRIV_REQUIRES unity
                    EMBED_TXTFILES certs/test_rsa_private_key.pem
                    EMBED_FILES image.bin
                    WHOLE_ARCHIVE)
