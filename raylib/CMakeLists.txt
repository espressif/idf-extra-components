idf_component_register(
    SRCS
        # Core Raylib source files (minimal set for 2D rendering)
        "raylib/src/rcore.c"
        "raylib/src/rtext.c"
        "raylib/src/rtextures.c"
        "raylib/src/rshapes.c"
        # NOTE: rmodels.c excluded - requires file system APIs not available on embedded
        # NOTE: utils.c removed in raylib 6.0

        # Platform-specific implementation
        "src/platforms/rcore_esp_idf.c"
    INCLUDE_DIRS
        "include"                # Our wrapper includes (stubs) - custom config.h here takes precedence
        "raylib/src"
        "raylib/src/external"
    PRIV_REQUIRES
        esp_lcd
)

# Enable software renderer
# RLSW_FRAMEBUFFER_COLOR_TYPE/RGB565 must be defined on the command line so it
# reaches the rlsw software-renderer translation unit (rlsw.h, which does not
# include config.h). config.h also defines it identically, which is benign.
target_compile_definitions(${COMPONENT_TARGET} PUBLIC
    GRAPHICS_API_OPENGL_SOFTWARE
    PLATFORM_CUSTOM
    RLSW_FRAMEBUFFER_COLOR_TYPE=R5G6B5
    RLSW_FRAMEBUFFER_OUTPUT_BGRA=false
)

# Prioritize our custom include directory for config.h lookup and suppress warnings
target_compile_options(${COMPONENT_TARGET} PRIVATE
    -I${CMAKE_CURRENT_LIST_DIR}/include          # Custom config.h takes precedence
    -Wno-incompatible-pointer-types              # M3D and software renderer compatibility
    -Wno-uninitialized
    -Wno-nonnull
    -Wno-return-type
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-unused-label                            # Software renderer (rlsw.h) has unused labels
    -Wno-stringop-truncation                     # Raylib rtext.c TextReplaceBetween
)

