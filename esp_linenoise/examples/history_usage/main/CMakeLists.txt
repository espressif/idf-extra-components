idf_build_get_property(target IDF_TARGET)
set(priv_requires esp_linenoise)

if(NOT "${target}" STREQUAL "linux")
    list(APPEND priv_requires spiffs)
endif()

idf_component_register(
    SRCS "history_usage.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES ${priv_requires}
)
