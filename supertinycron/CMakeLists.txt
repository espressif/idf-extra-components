idf_component_register(SRCS "supertinycron/ccronexpr.c"
                    INCLUDE_DIRS "supertinycron/")

target_compile_definitions(${COMPONENT_TARGET} PRIVATE "-DCRON_USE_LOCAL_TIME")
if(CONFIG_CRON_DISABLE_YEARS)
    target_compile_definitions(${COMPONENT_TARGET} PRIVATE "-DCRON_DISABLE_YEARS")
endif()
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-char-subscripts)
