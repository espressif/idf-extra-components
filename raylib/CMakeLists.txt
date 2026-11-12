idf_component_register(
    SRCS "src/esp_raylib.c" "src/esp_raylib_memory.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    PRIV_REQUIRES esp_timer vfs heap log esp_psram
    )

set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raylib")

# takes PLATFORM_MEMORY, silencing the no-platform #pragma message.
set(PLATFORM  "Memory")

# Shadow rcore.c's platform hook: its quoted include
# "platforms/rcore_memory.c" resolves against the includer's own directory,
# so a copy of rcore.c placed here picks up a file we control at that path.
set(_rcore_shadow "${CMAKE_CURRENT_BINARY_DIR}/rcore_shadow")
configure_file("${RAYLIB_DIR}/src/rcore.c" "${_rcore_shadow}/rcore.c" COPYONLY)
# That file is a one-line forwarding stub, not a copy of the port. Its content
# depends only on where this component lives, never on the port's contents, so
# editing src/rcore_platform_esp_idf.c changes no generated file and triggers
# no CMake reconfigure. The compiler's depfile records the real source as an
# input to rcore.c, so Ninja simply rebuilds that one object.
set(_port_source "${CMAKE_CURRENT_SOURCE_DIR}/src/rcore_platform_esp_idf.c")
file(TO_CMAKE_PATH "${_port_source}" _port_source)
file(CONFIGURE OUTPUT "${_rcore_shadow}/platforms/rcore_memory.c"
     CONTENT "#include \"@_port_source@\"\n" @ONLY)

# Compile the copy by path, never the submodule's.
target_sources(${COMPONENT_LIB} PRIVATE "${_rcore_shadow}/rcore.c")

set(BUILD_EXAMPLES    OFF)
set(BUILD_SHARED_LIBS OFF)

# IDF takes optimization and assertions from sdkconfig, not CMAKE_BUILD_TYPE.
# Give upstream a value so its src/CMakeLists.txt does not FORCE "Debug" into
# the global CACHE (which would leak into every other IDF component).
set(CMAKE_BUILD_TYPE Debug)

# This target has no window system
set(USE_EXTERNAL_GLFW OFF)

# Upstream's install() rules would write into the IDF prefix, and
# install(EXPORT) validates a link interface that cannot resolve here,
# so they are suppressed for the duration of the subdirectory.
set(_raylib_skip_install "${CMAKE_SKIP_INSTALL_RULES}")
set(CMAKE_SKIP_INSTALL_RULES ON)
add_subdirectory("${RAYLIB_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/raylib")
set(CMAKE_SKIP_INSTALL_RULES "${_raylib_skip_install}")
unset(_raylib_skip_install)

# rcore.c is compiled with the ESP-IDF port in this component instead.
get_target_property(_raylib_sources raylib SOURCES)
list(FILTER _raylib_sources EXCLUDE REGEX "rcore\\.c$")
set_target_properties(raylib PROPERTIES SOURCES "${_raylib_sources}")

target_link_libraries(${COMPONENT_LIB} PUBLIC raylib)

# These are injected as -D, preempting config.h's #ifndef defaults
set(_raylib_defs
    # Two buffer slots so the platform layer hands a finished frame to the
    # panel while the renderer draws into the other one. The framebuffer pool
    # holds render textures only, so this is two BeginTextureMode() targets.
    RLSW_DOUBLE_BUFFERING=1
    RLSW_MAX_FRAMEBUFFERS=2
    RLSW_MAX_TEXTURES=32

    # No audio and no 3D model loader.
    SUPPORT_MODULE_RMODELS=0
    SUPPORT_MODULE_RAUDIO=0

    # DDS mostly carries GPU-compressed textures the software renderer cannot sample
    SUPPORT_FILEFORMAT_DDS=0

    # no capture, automation, gestures or clipboard hardware to drive.
    SUPPORT_TRACELOG=0
    SUPPORT_CAMERA_SYSTEM=0
    SUPPORT_GESTURES_SYSTEM=0
    SUPPORT_MOUSE_GESTURES=0
    SUPPORT_SCREEN_CAPTURE=0
    SUPPORT_COMPRESSION_API=0
    SUPPORT_AUTOMATION_EVENTS=0
    SUPPORT_CLIPBOARD_IMAGE=0

    # Nothing on this target exports or procedurally generates images: keep
    # stb_image_write and the GenImage* family out of the binary.
    SUPPORT_IMAGE_EXPORT=0
    SUPPORT_IMAGE_GENERATION=0

    # MCU paths and one screen, not a desktop window.
    MAX_FILEPATH_CAPACITY=256
    MAX_FILEPATH_LENGTH=128
    MAX_KEY_PRESSED_QUEUE=4
    MAX_CHAR_PRESSED_QUEUE=4
    MAX_TOUCH_POINTS=1
    MAX_GAMEPADS=1
    MAX_TEXT_BUFFER_LENGTH=512
    MAX_TEXTSPLIT_COUNT=32

    # One draw-call list that fits in internal RAM, half the texture units.
    RL_DEFAULT_BATCH_DRAWCALLS=128
    RL_DEFAULT_BATCH_MAX_TEXTURE_UNITS=2

    RLSW_FRAMEBUFFER_COLOR_TYPE=${CONFIG_RAYLIB_FRAMEBUFFER_COLOR_TYPE}

    # Only 3D drawing through BeginMode3D()/rlgl touches the depth buffer,
    # and this component is 2D-first.
    RLSW_FRAMEBUFFER_DEPTH_TYPE=D8
)

foreach(_fmt QOI PNG BMP TGA JPG GIF TTF FNT)
    if(CONFIG_RAYLIB_FILEFORMAT_${_fmt})
        list(APPEND _raylib_defs SUPPORT_FILEFORMAT_${_fmt}=1)
    else()
        list(APPEND _raylib_defs SUPPORT_FILEFORMAT_${_fmt}=0)
    endif()
endforeach()

foreach(_target raylib ${COMPONENT_LIB})
    target_compile_definitions(${_target} PRIVATE ${_raylib_defs})
    target_compile_options(${_target} PRIVATE
        # Renames raylib's RL_* allocators to ours and declares them in every upstream TU
        "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/esp_raylib_memory.h"
        # Upstream warnings we cannot fix in the submodule (rlsw.h/rlgl.h noise).
        # TODO: drop once fixed upstream.
        -Wno-unused-variable
        -Wno-unused-function
        -Wno-incompatible-pointer-types
        -Wno-unused-but-set-variable
        # raylib is self-contained and safe at -O2 regardless.
        -O2
    )
endforeach()
