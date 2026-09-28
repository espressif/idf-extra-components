idf_component_register(SRCS "test_main.c"
                            "test_libjpeg_turbo.c"
                       PRIV_REQUIRES unity
                       EMBED_FILES image.jpg image32x32.jpg
                       WHOLE_ARCHIVE)
