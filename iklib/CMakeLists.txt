idf_component_register(
                       # We need the dummy source file so that the component
                       # library is not an interface library. This allows to
                       # get the list of include directories from other components
                       # via INCLUDE_DIRECTORIES property later on.
                       SRCS dummy.c)

# Determine compilation flags used for building ik (inverse kinematics) library
# Flags inherited from IDF build system and other IDF components:
set(idf_include_directories $<TARGET_PROPERTY:idf::iklib,INCLUDE_DIRECTORIES>)
set(includes "-I$<JOIN:${idf_include_directories}, -I>")

set(c_flags "${includes} ${extra_defines} ")
set(cxx_flags "${includes} ${extra_defines} ")
set(common_flags "-ffunction-sections -fdata-sections -fpermissive -lm -Wno-error=enum-int-mismatch -Wno-error=format")

if(CONFIG_IDF_TARGET_ARCH_XTENSA)
    set(assert_flags "${assert_flags} -mlongcalls ")
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

# Build ik in this directory:
set(BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/iklib-build)

set(lib_path ${BINARY_DIR}/install/lib/ik.a)
add_prebuilt_library(iklib ${lib_path})

# We apply the patch to the ik library to fix the build error.
find_package(Git REQUIRED)
execute_process(
    COMMAND ${GIT_EXECUTABLE} -C ${CMAKE_CURRENT_SOURCE_DIR}/ik apply --ignore-whitespace ${CMAKE_CURRENT_SOURCE_DIR}/patch/ik_patch.patch
    RESULT_VARIABLE GIT_PATCH_RESULT
)

if(NOT GIT_PATCH_RESULT EQUAL "0")
    message(WARNING "Failed to apply patch! It's possible that the patch is already applied.")
endif()

# Add jpeg-turbo as a subproject.
ExternalProject_Add(iklib_proj
    SOURCE_DIR ${COMPONENT_DIR}/ik
    BINARY_DIR ${BINARY_DIR}
    BUILD_BYPRODUCTS ${lib_path}
    # These two options are set so that Ninja immediately outputs
    # the subproject build to the terminal. Otherwise it looks like the
    # build process "hangs" for too long until iklib build is complete.
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
    CMAKE_POSITION_INDEPENDENT_CODE ON
    # Arguments to pass to iklib CMake invocation:
    CMAKE_ARGS
        -DCMAKE_C_FLAGS=${c_flags}
        -DCMAKE_CXX_FLAGS=${cxx_flags}
        ${opt_args}
        -DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/install
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DCMAKE_SYSTEM_PROCESSOR=esp32
        -DENABLE_SHARED=NO
        -DENABLE_STATIC=YES
        -DIK_PRECISION=float
        -DIK_PROFILING=NO
)

# Attach header files to the component library:
set_target_properties(${COMPONENT_LIB} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${BINARY_DIR}/install/include)

# Make sure the subproject is built before the component library:
add_dependencies(${COMPONENT_LIB} iklib_proj)

# Finally, link the interface library to the component library:
# Attach IDF compoenent dependencies to jpeg libraries
foreach(dep ${deps})
    target_link_libraries(iklib INTERFACE idf::${dep})
endforeach()
# Attach jpeg-turbo libraries to the component library
target_link_libraries(${COMPONENT_LIB} INTERFACE iklib)

