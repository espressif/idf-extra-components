idf_component_register(
    SRCS test_quirc.c test_main.c
    PRIV_REQUIRES unity
    EMBED_FILES test_qrcode.pgm
    WHOLE_ARCHIVE)
