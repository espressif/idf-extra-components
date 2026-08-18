idf_component_register()

# Override options defined in brotli/CMakeLists.txt.
# Brotli builds static libraries when BUILD_SHARED_LIBS is OFF. We only need the
# C library, so we build it as a static library and disable everything else
# (CLI tools, tests, install, docs, pkg-config, etc.).
set(BUILD_SHARED_LIBS OFF)
set(BROTLI_DISABLE_TESTS ON)
set(BROTLI_BUILD_TOOLS OFF CACHE BOOL "" FORCE)

# The upstream probe accepts an empty translation unit when __EMSCRIPTEN__ is
# undefined, incorrectly detecting the ESP-IDF toolchain as Emscripten.
# Cache the known result before entering the upstream directory so its probe is not run.
set(BROTLI_EMSCRIPTEN OFF CACHE INTERNAL "ESP-IDF is not Emscripten" FORCE)

# When brotli is added as a subdirectory it automatically enters "bundled
# mode" (BROTLI_BUNDLED_MODE=ON) because it is not the top-level project, which
# skips the install() rules.
add_subdirectory(brotli)

# Brotli unconditionally enables PIC for its libraries on non-Emscripten
# toolchains. ESP-IDF targets cannot link objects built with -fPIC/-fPIE:
# the target linker script discards the resulting .got.plt section.
foreach(lib brotlicommon brotlidec brotlienc)
    set_target_properties(${lib} PROPERTIES POSITION_INDEPENDENT_CODE FALSE)
endforeach()

# Old ESP-IDF GCC toolchains report false positives in Brotli's macro-heavy
# decoder after optimization; ESP-IDF promotes these warnings to errors.
target_compile_options(brotlidec PRIVATE -Wno-maybe-uninitialized)

# The Brotli C library is split into three static libraries (brotlicommon,
# brotlidec, brotlienc). We link all of them into this component so that any
# project depending on espressif/brotli gets the complete C API.
target_link_libraries(${COMPONENT_LIB} INTERFACE brotlidec brotlienc brotlicommon)
