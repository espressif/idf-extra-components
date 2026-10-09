idf_component_register(SRCS "src/json_generator.c"
                    INCLUDE_DIRS "include"
                    )

# The "nano" printf has no 64-bit conversions; the component formats int64_t
# itself then (the old option name covers ESP-IDF 5.x)
if(CONFIG_LIBC_NEWLIB_NANO_FORMAT OR CONFIG_NEWLIB_NANO_FORMAT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE JSON_GEN_PRINTF_HAS_INT64=0)
endif()
