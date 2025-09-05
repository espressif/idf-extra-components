idf_component_register(
    SRCS "src/esp_isotp.c" "isotp-c/isotp.c"
    INCLUDE_DIRS "inc"
    PRIV_INCLUDE_DIRS "isotp-c"
    REQUIRES esp_driver_twai
    PRIV_REQUIRES esp_timer
)

# Force include our custom isotp_config.h before any source file compilation
# This ensures our configuration is loaded first, preventing macro redefinitions
target_compile_options(${COMPONENT_LIB} PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/isotp_config.h"
)
