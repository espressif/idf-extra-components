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

set(c_flags "${includes} ${extra_defines} ")
set(cxx_flags "${includes} ${extra_defines} ")
set(common_flags "-ggdb  -ffunction-sections -fdata-sections -fpermissive -lm")

if(CONFIG_IDF_TARGET_ARCH_XTENSA)
    set(assert_flags "${assert_flags} -mlongcalls")
endif()

# We redefine the flags to apply common flags, 
# like -ffunction-sections -fdata-sections.
if(CONFIG_COMPILER_OPTIMIZATION_DEFAULT)
    set(opt_flags "-Og ${common_flags} ${assert_flags}")
    set(opt_args -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_C_FLAGS_RELEASE=${opt_flags}
    -DCMAKE_CXX_FLAGS_RELEASE=${opt_flags})
elseif(CONFIG_COMPILER_OPTIMIZATION_SIZE)
    set(opt_flags "-Os ${common_flags} ${assert_flags}")
    set(opt_args -DCMAKE_BUILD_TYPE=MinSizeRel
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
    message(FATAL_ERROR "Unsupported optimization level")
endif()

include(ExternalProject)

# Build jpeg-turbo in this directory:
set(BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/libjpeg-build)

set(lib_path ${BINARY_DIR}/install/lib/libjpeg.a)
add_prebuilt_library(libjpeg-turbo ${lib_path})

# Add jpeg-turbo as a subproject.
ExternalProject_Add(jpegturbo_proj
    SOURCE_DIR ${COMPONENT_DIR}/libjpeg-turbo
    BINARY_DIR ${BINARY_DIR}
    BUILD_BYPRODUCTS ${lib_path}
    # These two options are set so that Ninja immediately outputs
    # the subproject build to the terminal. Otherwise it looks like the
    # build process "hangs" for too long until jpeg-turbo build is complete.
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
    CMAKE_POSITION_INDEPENDENT_CODE ON
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

# Attach header files to the component library:
set_target_properties(${COMPONENT_LIB} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${BINARY_DIR}/install/include)

# Make sure the subproject is built before the component library:
add_dependencies(${COMPONENT_LIB} jpegturbo_proj)

# Finally, link the interface library to the component library:
# Attach IDF compoenent dependencies to jpeg libraries
foreach(dep ${deps})
    target_link_libraries(libjpeg-turbo INTERFACE idf::${dep})
endforeach()
# Attach jpeg-turbo libraries to the component library
target_link_libraries(${COMPONENT_LIB} INTERFACE libjpeg-turbo)
