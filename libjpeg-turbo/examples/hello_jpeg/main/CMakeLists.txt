idf_component_register(SRCS "main.c" "decode_image.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES image.jpg
                    REQUIRES esp_timer)
