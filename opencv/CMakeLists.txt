idf_component_register(
                       # We need the dummy source file so that the component
                       # library is not an interface library. This allows to
                       # get the list of include directories from other components
                       # via INCLUDE_DIRECTORIES property later on.
                       SRCS dummy.c
                       # OpenCV librarires depend on some of the IDF libraries,
                       # add them here:
                       PRIV_REQUIRES vfs lwip pthread newlib)

# Determine compilation flags used for building OpenCV
# Flags inherited from IDF build system and other IDF components:
set(idf_include_directories $<TARGET_PROPERTY:idf::opencv,INCLUDE_DIRECTORIES>)
set(includes "-I$<JOIN:${idf_include_directories}, -I>")
# IDF is built with -D_GNU_SOURCE, but we don't actually implement all _GNU_SOURCE
# features. Defining _GNU_SOURCE causes OpenCV to try to use them, which fails.
# Use _DEFAULT_SOURCE instead, it provides enough features to build OpenCV.
set(extra_defines -D_DEFAULT_SOURCE)
# Final flags for C and C++:
set(c_flags "${includes} ${extra_defines}")
set(cxx_flags "${includes} ${extra_defines}")

set(common_flags "-ggdb  -ffunction-sections -fdata-sections")

# Calculate flags for optimizations, assertions, debug info:
if(COMPILER_OPTIMIZATION_ASSERTIONS_DISABLE)
   set(assert_flags "-DNDEBUG")
else()
   set(assert_flags "-DDEBUG -D_DEBUG")
endif()

if(CONFIG_IDF_TARGET_ARCH_XTENSA)
    set(assert_flags "${assert_flags} -mlongcalls")
endif()

if(CONFIG_COMPILER_OPTIMIZATION_DEFAULT)
    set(opt_flags "-Og ${common_flags} ${assert_flags}")
    set(opt_args -DCMAKE_BUILD_TYPE=Debug
                 -DCMAKE_C_FLAGS_DEBUG=${opt_flags}
                 -DCMAKE_CXX_FLAGS_DEBUG=${opt_flags})
elseif(CONFIG_COMPILER_OPTIMIZATION_SIZE)
    # Normally we should use MinSizeRel build type for this,
    # however OpenCV CMake files only handle Debug and Release
    # (see e.g. opencv/cmake/OpenCVCompilerOptions.cmake).
    # So we redefine the flags for Release instead.
    set(opt_flags "-Os ${common_flags} ${assert_flags}")
    set(opt_args -DCMAKE_BUILD_TYPE=Release
                 -DCMAKE_C_FLAGS_RELEASE=${opt_flags}
                 -DCMAKE_CXX_FLAGS_RELEASE=${opt_flags})
elseif(COMPILER_OPTIMIZATION_PERF)
    # Currently OpenCV fails to compile at -O2 level on Xtensa due to a compiler issue,
    # details in https://github.com/jcmvbkbc/gcc-xtensa/issues/14.
    # Compiling at -O3 happens to be okay.
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

# Python to use for OpenCV build
idf_build_get_property(python PYTHON)

include(ExternalProject)

# Build OpenCV in this directory:
set(BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/opencv-build)

# List of OpenCV static libraries we expect to see after the build.
# Obtained manually by looking at ${BINARY_DIR}/install/lib directory.
# In some cases it's important to have right order of libraries here.
set(opencv_libraries
    calib3d flann fuzzy 
    objdetect tracking video videostab features2d 
    surface_matching
    shape stereo stitching structured_light
    line_descriptor ml phase_unwrapping photo plot quality rapid
    imgcodecs imgproc core
)

set(3rdparty_libraries
    zlib 
    libjasper 
    libjpeg-turbo 
    libpng 
    libtiff 
    libwebp 
)

# List of all static libraries to be produced
set(all_libraries)
foreach(libname ${opencv_libraries})
    set(lib_path ${BINARY_DIR}/install/lib/libopencv_${libname}.a)
    list(APPEND all_libraries ${lib_path})
endforeach()
foreach(libname ${3rdparty_libraries})
    set(lib_path ${BINARY_DIR}/install/lib/opencv4/3rdparty/lib${libname}.a)
    list(APPEND all_libraries ${lib_path})
endforeach()

# Add OpenCV as a subproject.
ExternalProject_Add(opencv_proj
    SOURCE_DIR ${COMPONENT_DIR}/opencv
    BINARY_DIR ${BINARY_DIR}
    BUILD_BYPRODUCTS ${all_libraries}
    # These two options are set so that Ninja immediately outputs
    # the subproject build to the terminal. Otherwise it looks like the
    # build process "hangs" for too long until OpenCV build is complete.
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
    # Arguments to pass to OpenCV CMake invocation:
    CMAKE_ARGS
        -DCMAKE_C_FLAGS=${c_flags}
        -DCMAKE_CXX_FLAGS=${cxx_flags}
        ${opt_args}
        -DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/install
        -DENABLE_CCACHE=${CCACHE_ENABLE}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DPYTHON_DEFAULT_EXECUTABLE=${python}
        -DCV_CPU_BASELINE_MODE=${CONFIG_IDF_TARGET}
        -DCV_DISABLE_OPTIMIZATION=0
        -DOPENCV_EXTRA_MODULES_PATH=${CMAKE_CURRENT_LIST_DIR}/opencv_contrib/modules
        -DOPENCV_FORCE_3RDPARTY_BUILD=1
        -DCMAKE_SYSTEM_PROCESSOR=${CONFIG_IDF_TARGET_ARCH}
        -DENABLE_PIC=0
        -DTARGET=${CONFIG_IDF_TARGET}
        -DWITH_ADE=0
        -DWITH_JPEG=1
        -DWITH_OPENCL=0
        -DWITH_OPENEXR=0
        -DWITH_OPENGL=0
        -DWITH_OPENMP=0
        -DWITH_PNG=1
        -DWITH_PROTOBUF=0
        -DWITH_TIFF=1
        -DWITH_WEBP=1
        -DBUILD_EXAMPLES=0
        -DBUILD_PACKAGE=1
        -DBUILD_PERF_TESTS=0
        -DBUILD_PNG=1
        -DBUILD_SHARED_LIBS=0
        -DBUILD_TESTS=0
        -DBUILD_ZLIB=1
        -DBUILD_opencv_apps=0
        -DBUILD_opencv_bgsegm=0
        -DBUILD_opencv_datasets=0
        -DBUILD_opencv_imgcodecs=1
        -DBUILD_opencv_objdetect=1
        -DBUILD_opencv_imgproc=1
        -DBUILD_opencv_tracking=1
        -DBUILD_opencv_video=1
        -DBUILD_opencv_videoio=0
)

add_dependencies(${COMPONENT_LIB} opencv_proj)

# Attach generated libraries and header files to an interface library:
add_library(opencv_interface_lib INTERFACE)
set_target_properties(opencv_interface_lib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${BINARY_DIR}/install/include/opencv4)
set_target_properties(opencv_interface_lib PROPERTIES INTERFACE_LINK_LIBRARIES "${all_libraries}")
add_dependencies(opencv_interface_lib opencv_proj)

# Finally, link the interface library to the component library:
target_link_libraries(${COMPONENT_LIB} INTERFACE opencv_interface_lib)
