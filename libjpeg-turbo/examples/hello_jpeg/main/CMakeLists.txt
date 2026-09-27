idf_component_register(SRCS "hello_jpeg_example_main.c"
                       PRIV_REQUIRES esp_psram
                       EMBED_FILES image32x32.jpg image.jpg)
