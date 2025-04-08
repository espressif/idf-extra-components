idf_component_register(
                       # We need the dummy source file so that the component
                       # library is not an interface library. This allows to
                       # get the list of include directories from other components
                       # via INCLUDE_DIRECTORIES property later on.
                       SRCS dummy.c)

# Determine compilation flags used for building Jpeg-turbo
# Flags inherited from IDF build system and other IDF components:
set(idf_include_directories $<TARGET_PROPERTY:idf::libjpeg-turbo,INCLUDE_DIRECTORIES>)
set(includes "-I$<JOIN:${idf_include_directories}, -I>")


if(CONFIG_COMPILER_OPTIMIZATION_DEFAULT)
    set(opt_args -DCMAKE_BUILD_TYPE=Debug)
elseif(CONFIG_COMPILER_OPTIMIZATION_SIZE)
    set(opt_args -DCMAKE_BUILD_TYPE=Relesee)
elseif(CONFIG_COMPILER_OPTIMIZATION_PERF)
    set(opt_args -DCMAKE_BUILD_TYPE=Relesee)
elseif(CONFIG_COMPILER_OPTIMIZATION_NONE)
    set(opt_args -DCMAKE_BUILD_TYPE=Debug )
else()
    message(FATAL_ERROR "Unsupported optimization level")
endif()

set(extra_defines -D_DEFAULT_SOURCE)

set(c_flags "${includes} ${extra_defines} ")
set(cxx_flags "${includes} ${extra_defines} ")
set(common_flags "-ggdb  -ffunction-sections -fdata-sections -fpermissive -lm")

# set(COMPILE_FLAGS "-ggdb  -ffunction-sections -fdata-sections -fpermissive -lm")
# set(COMPILE_FLAGS "-ggdb  -ffunction-sections -fdata-sections -fpermissive")

if(CONFIG_IDF_TARGET_ARCH_XTENSA)
    set(assert_flags "${assert_flags} -mlongcalls")
endif()

if(CONFIG_COMPILER_OPTIMIZATION_DEFAULT)
    set(opt_flags "-Og ${common_flags} ${assert_flags}")
    set(opt_args -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_C_FLAGS_RELEASE=${opt_flags}
    -DCMAKE_CXX_FLAGS_RELEASE=${opt_flags})
elseif(CONFIG_COMPILER_OPTIMIZATION_SIZE)
    # We redefine the flags for Release instead.
    set(opt_flags "-Os ${common_flags} ${assert_flags}")
    set(opt_args -DCMAKE_BUILD_TYPE=Release
                 -DCMAKE_C_FLAGS_RELEASE=${opt_flags}
                 -DCMAKE_CXX_FLAGS_RELEASE=${opt_flags})
elseif(CONFIG_COMPILER_OPTIMIZATION_PERF)   
    set(opt_flags "-O3 ${common_flags} ${assert_flags}")
    set(opt_args -DCMAKE_BUILD_TYPE=Release
                 -DCMAKE_C_FLAGS_RELEASE=${opt_flags}
                 -DCMAKE_CXX_FLAGS_RELEASE=${opt_flags})
elseif(COMPILER_OPTIMIZATION_NONE)
    set(opt_flags "-O0 ${common_flags} ${assert_flags}")
    set(opt_args -DCMAKE_BUILD_TYPE=Debug
                 -DCMAKE_C_FLAGS_DEBUG=${opt_flags}
                 -DCMAKE_CXX_FLAGS_DEBUG=${opt_flags})
else()
    set(opt_flags "-O0 ${common_flags} ${assert_flags}")
    set(opt_args -DCMAKE_BUILD_TYPE=Debug
                 -DCMAKE_C_FLAGS_DEBUG=${opt_flags}
                 -DCMAKE_CXX_FLAGS_DEBUG=${opt_flags})
endif()

idf_build_get_property(python PYTHON)

include(ExternalProject)

# Build jpeg-turbo in this directory:
set(BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/libjpeg-build)

# The list here just to make it common with other components
set(jpegturbo_libraries
    libjpeg
)

set(all_libraries)
set(all_targets)

foreach(libname ${jpegturbo_libraries})
    set(lib_path ${BINARY_DIR}/install/lib/${libname}.a)
    list(APPEND all_libraries ${lib_path})
    list(APPEND all_targets libjpeg_${libname})
    add_prebuilt_library(libjpeg_${libname} ${lib_path})
endforeach()

# Add jpeg-turbo as a subproject.
ExternalProject_Add(jpegturbo_proj
    SOURCE_DIR ${COMPONENT_DIR}/libjpeg-turbo
    BINARY_DIR ${BINARY_DIR}
    BUILD_BYPRODUCTS ${all_libraries}
    # These two options are set so that Ninja immediately outputs
    # the subproject build to the terminal. Otherwise it looks like the
    # build process "hangs" for too long until jpeg-turbo build is complete.
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
    # Arguments to pass to jpeg-turbo CMake invocation:
    CMAKE_ARGS
        -DCMAKE_C_FLAGS=${c_flags}
        -DCMAKE_CXX_FLAGS=${cxx_flags}
        ${opt_args}
        -DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/install
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DWITH_TURBOJPEG=NO
        -DWITH_SIMD=NO
        -DCMAKE_SYSTEM_PROCESSOR=esp32 # Without this parameter the lib make an error
        -DWITH_ARITH_DEC=YES
        -DWITH_ARITH_ENC=YES
        -DWITH_JPEG8=YES
        -DWITH_JPEG7=YES
        -DENABLE_SHARED=NO
        -DENABLE_STATIC=YES        
)

add_dependencies(${COMPONENT_LIB} jpegturbo_proj)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Attach header files to the component library:
set_target_properties(${COMPONENT_LIB} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${BINARY_DIR}/install/include)

# Make sure the subproject is built before the component library:
add_dependencies(${COMPONENT_LIB} jpegturbo_proj)

# Finally, link the interface library to the component library:
foreach(libjpeg_target ${all_targets})
    # Attach IDF compoenent dependencies to jpeg libraries
    foreach(dep ${deps})
        target_link_libraries(${libjpeg_target} INTERFACE idf::${dep})
    endforeach()
    # Attach jpeg-turbo libraries to the component library
    target_link_libraries(${COMPONENT_LIB} INTERFACE ${libjpeg_target})
endforeach()
