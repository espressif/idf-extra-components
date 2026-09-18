idf_component_register(SRCS "src/xmodem.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_timer esp_hw_support)

if (COMMAND idf_define_esp_err_codes)
    idf_define_esp_err_codes(HEADERS include/xymodem.h)
endif()
