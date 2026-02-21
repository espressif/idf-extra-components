
idf_build_get_property(target IDF_TARGET)
set(priv_requires esp_linenoise)

if(NOT "${target}" STREQUAL "linux")
    list(APPEND priv_requires esp_driver_uart vfs)
endif()

idf_component_register(
    SRCS "multi_instance.c" "../../utils/common_io.c"
    INCLUDE_DIRS "." "../../utils"
    PRIV_REQUIRES ${priv_requires}
)
