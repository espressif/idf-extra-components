idf_component_register(SRCS "ccronexpr/ccronexpr.c"
                    INCLUDE_DIRS "ccronexpr/")

target_compile_definitions(${COMPONENT_TARGET} PRIVATE "-DCRON_USE_LOCAL_TIME")
if(CONFIG_CRON_DISABLE_YEARS)
    target_compile_definitions(${COMPONENT_TARGET} PRIVATE "-DCRON_DISABLE_YEARS")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-char-subscripts")
