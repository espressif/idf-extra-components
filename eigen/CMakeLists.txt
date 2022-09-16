idf_component_register(
                       # We need the dummy source file so that the component
                       # library is not an interface library. This allows to
                       # get the list of include directories from other components
                       # via INCLUDE_DIRECTORIES property later on.
                       SRCS dummy.c)

# Determine compilation flags used for building Eigen
# Flags inherited from IDF build system and other IDF components:
set(idf_include_directories $<TARGET_PROPERTY:idf::eigen,INCLUDE_DIRECTORIES>)
set(includes "-I$<JOIN:${idf_include_directories}, -I>")

if(CONFIG_COMPILER_OPTIMIZATION_DEFAULT)
    set(opt_args -DCMAKE_BUILD_TYPE=Debug)
elseif(CONFIG_COMPILER_OPTIMIZATION_SIZE)
    set(opt_args -DCMAKE_BUILD_TYPE=Relesee)
elseif(COMPILER_OPTIMIZATION_PERF)
    set(opt_args -DCMAKE_BUILD_TYPE=Relesee)
elseif(COMPILER_OPTIMIZATION_NONE)
    set(opt_args -DCMAKE_BUILD_TYPE=Debug)
else()
    message(FATAL_ERROR "Unsupported optimization level")
endif()

include(ExternalProject)

# Build Eigen in this directory:
set(BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/eigen-build)

# Add Eigen as a subproject.
ExternalProject_Add(eigen_proj
    SOURCE_DIR ${COMPONENT_DIR}/eigen
    BINARY_DIR ${BINARY_DIR}
    # These two options are set so that Ninja immediately outputs
    # the subproject build to the terminal. Otherwise it looks like the
    # build process "hangs" for too long until Eigen build is complete.
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
    # Arguments to pass to Eigen CMake invocation:
    CMAKE_ARGS
        ${opt_args}
        -DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/install
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
)

add_dependencies(${COMPONENT_LIB} eigen_proj)

# Attach generated libraries and header files to an interface library:
add_library(eigen_interface_lib INTERFACE)
set_target_properties(eigen_interface_lib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${BINARY_DIR}/install/include)
add_dependencies(eigen_interface_lib eigen_proj)

# Finally, link the interface library to the component library:
target_link_libraries(${COMPONENT_LIB} INTERFACE eigen_interface_lib)
