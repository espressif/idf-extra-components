# Upstream libjpeg-turbo aborts unless it is the top-level project and
# recommends ExternalProject_Add().
idf_component_register()

include(ExternalProject)

set(jpegturbo_binary_dir ${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo-build)
set(jpegturbo_install_dir ${jpegturbo_binary_dir}/install)
set(jpegturbo_library ${jpegturbo_install_dir}/lib/libjpeg.a)

# Headers land in the install tree only after the sub-build. Create the
# directory now so INTERFACE_INCLUDE_DIRECTORIES is valid at configure time.
file(MAKE_DIRECTORY ${jpegturbo_install_dir}/include)

# The toolchain file already provides the cross compiler and architecture
# flags (on IDF 6, via the @cflags response file). Forward the options IDF
# computed from sdkconfig so the sub-build matches the rest of the project
# (optimization, -ffunction-sections, debug info, ...).
#

idf_build_get_property(jpegturbo_compile_options COMPILE_OPTIONS)
idf_build_get_property(jpegturbo_c_compile_options C_COMPILE_OPTIONS)
idf_build_get_property(jpegturbo_compile_definitions COMPILE_DEFINITIONS)
set(jpegturbo_c_flags ${jpegturbo_compile_options} ${jpegturbo_c_compile_options})
foreach(def IN LISTS jpegturbo_compile_definitions)
    if(NOT def MATCHES "^-D")
        set(def "-D${def}")
    endif()
    list(APPEND jpegturbo_c_flags "${def}")
endforeach()
# Put the flags in a gcc response file, one flag per line
set(jpegturbo_cflags_file ${CMAKE_CURRENT_BINARY_DIR}/jpegturbo-cflags)
list(JOIN jpegturbo_c_flags "\n" jpegturbo_c_flags)
file(WRITE ${jpegturbo_cflags_file} "${jpegturbo_c_flags}\n")

ExternalProject_Add(jpegturbo_proj
    SOURCE_DIR ${COMPONENT_DIR}/libjpeg-turbo
    BINARY_DIR ${jpegturbo_binary_dir}
    INSTALL_DIR ${jpegturbo_install_dir}
    BUILD_BYPRODUCTS ${jpegturbo_library}
    # Ninja otherwise looks idle until the whole sub-build finishes.
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
    CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
        -DCMAKE_BUILD_TYPE=Release
        "-DCMAKE_C_FLAGS_RELEASE=@\"${jpegturbo_cflags_file}\""
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_POSITION_INDEPENDENT_CODE=OFF
        -DCMAKE_SYSTEM_PROCESSOR=${IDF_TARGET}
        -DENABLE_SHARED=OFF
        -DENABLE_STATIC=ON
        -DWITH_TURBOJPEG=OFF
        -DWITH_SIMD=OFF
        -DWITH_ARITH_DEC=ON
        -DWITH_ARITH_ENC=ON
        -DWITH_JPEG8=ON
        -DWITH_JPEG7=ON
        -DWITH_TOOLS=OFF
        -DWITH_TESTS=OFF
)

# IMPORTED location of a library that does not exist yet. BUILD_BYPRODUCTS
# plus this dependency tell Ninja to run the sub-build first.
add_prebuilt_library(jpegturbo ${jpegturbo_library})
add_dependencies(jpegturbo jpegturbo_proj)
target_include_directories(jpegturbo INTERFACE ${jpegturbo_install_dir}/include)

# Empty idf_component_register() creates an INTERFACE library, which is
# enough to export the prebuilt archive and its headers.
target_link_libraries(${COMPONENT_LIB} INTERFACE jpegturbo)
