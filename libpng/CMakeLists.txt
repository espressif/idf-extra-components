# Reuse upstream CMake instead of re-listing the sources.

idf_component_register(SRCS "png_memory_esp.c"
                       PRIV_REQUIRES heap)

# Only the static library and its public headers are needed;
# the shared library, tests, tools and demos are switched off.
set(PNG_STATIC ON)
set(PNG_SHARED OFF)
set(PNG_TESTS OFF)
set(PNG_TOOLS OFF)
# Keep upstream's install() rules out of the ESP-IDF install tree.
set(SKIP_INSTALL_ALL ON)

# Upstream SIMD is ARM NEON / x86 SSE / VSX / MSA / LSX / RVV. ESP cores
# do not implement those ISAs, and IDF's Generic toolchain may leave
# CMAKE_SYSTEM_PROCESSOR empty or set to the host, which would otherwise
# pick the host backend.
set(PNG_HARDWARE_OPTIMIZATIONS OFF)

# The version-script probe is only used by the shared library, and its
# try_compile links a hosted executable, which Generic/ESP-IDF cannot do.
set(ld-version-script OFF)

# Use upstream's prebuilt pnglibconf.h rather than generating it from
# pnglibconf.dfa, so the build does not depend on a host AWK. (Without this,
# upstream would run find_program(AWK) and generate if awk is on PATH, or
# silently fall back to the same prebuilt file if it is not.)
set(PNG_LIBCONF_HEADER "${COMPONENT_DIR}/libpng/scripts/pnglibconf.h.prebuilt"
    CACHE FILEPATH "" FORCE)

# Upstream is happy to be added as a subdirectory and does not require a top-level project.
add_subdirectory(libpng)

# png_static already exports png.h, the generated pnglibconf.h and zlib as
# PUBLIC. PUBLIC here gives those to png_memory_esp.c as well as to consumers;
# png_static stays a separate archive on the final link line.
target_link_libraries(${COMPONENT_LIB} PUBLIC png_static)

if(CONFIG_LIBPNG_PREFER_PSRAM)
    # png_create_png_struct() is the one point every read and write struct is
    # built through. The wrapped symbol is only referenced from inside
    # png_static, so -u pulls png_memory_esp.o in before that archive is scanned.
    target_link_libraries(${COMPONENT_LIB} INTERFACE
        "-Wl,--wrap=png_create_png_struct"
        "-Wl,-u,__wrap_png_create_png_struct")
endif()

# Some ESP-IDF components (for example FreeType) look libpng up through
# find_package(PNG). project_include.cmake keeps exposing the component as
# PNG::PNG for them.
