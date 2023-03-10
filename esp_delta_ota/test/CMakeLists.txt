idf_component_register(SRC_DIRS "."
                    PRIV_INCLUDE_DIRS "."
                    REQUIRES unity
                    PRIV_REQUIRES cmock esp_delta_ota
                    EMBED_FILES assets/base.bin assets/new.bin assets/patch.bin assets/bad_patch.bin)
