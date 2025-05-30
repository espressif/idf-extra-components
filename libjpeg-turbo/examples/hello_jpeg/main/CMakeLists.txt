idf_component_register(SRCS "main.c" "decode_image.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES image32x32.jpg image.jpg 
                    REQUIRES esp_timer)
