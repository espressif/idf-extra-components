idf_component_register(SRCS "l8w8jwt/src/base64.c"
        "l8w8jwt/src/claim.c"
        "l8w8jwt/src/decode.c"
        "l8w8jwt/src/encode.c"
        "l8w8jwt/src/util.c"
        "l8w8jwt/src/version.c"
    INCLUDE_DIRS "l8w8jwt/include"
    PRIV_INCLUDE_DIRS "port/private_include"
    PRIV_REQUIRES "mbedtls"
)

set_source_files_properties("l8w8jwt/src/encode.c" "l8w8jwt/src/decode.c"
                            PROPERTIES COMPILE_FLAGS "-Wno-maybe-uninitialized")

target_compile_options(${COMPONENT_LIB} PRIVATE "-DL8W8JWT_SMALL_STACK=1" "-DL8W8JWT_ENABLE_EDDSA=0")
