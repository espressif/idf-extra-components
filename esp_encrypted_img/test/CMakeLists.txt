idf_component_register(SRC_DIRS "."
                    PRIV_INCLUDE_DIRS "."
                    REQUIRES unity
                    PRIV_REQUIRES cmock esp_encrypted_img
                    EMBED_TXTFILES certs/test_rsa_private_key.pem
                    EMBED_FILES image.bin)
