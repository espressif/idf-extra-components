cmake_minimum_required(VERSION 3.10)

###
# Project definition
###
project(isotp LANGUAGES C VERSION 1.6.0 DESCRIPTION "A platform-agnostic ISOTP implementation in C for embedded devices.")

option(isotpc_USE_INCLUDE_DIR "Copy header files to separate include directory in current binary dir for better separation of header files to combat potential naming conflicts." OFF)
option(isotpc_STATIC_LIBRARY "Compile libisotpc as a static library, instead of a shared library." OFF)
option(isotpc_STATIC_LIBRARY_PIC "Make use of position independent code (PIC), when compiling as a static library (enabled automatically when building shared libraries)." OFF)
option(isotpc_PAD_CAN_FRAMES "Pad CAN frames to their full size." ON)
set(isotpc_CAN_FRAME_PAD_VALUE "0xAA" CACHE STRING "Padding byte value to be used in CAN frames if enabled")
option(isotpc_ENABLE_CAN_SEND_ARG "Adds an extra argument to isotp_user_send_can to better support multiple CAN interfaces." OFF)
option(isotpc_ENABLE_TRANSCEIVE_EVENTS "Enable events/callbacks for transmission and reception complete." OFF)
option(isotpc_ENABLE_TRANSMIT_COMPLETE_CALLBACK "Enable transmit complete callback." ON) # These can be disabled separately, as they are controlled by the main event option
option(isotpc_ENABLE_RECEIVE_COMPLETE_CALLBACK "Enable receive complete callback." ON) # These can be disabled separately, as they are controlled by the main event option
# option(isotpc_ENABLE_TESTING "Enable building of test suite." OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (isotpc_STATIC_LIBRARY OR MSVC)
    add_library(isotp STATIC ${CMAKE_CURRENT_SOURCE_DIR}/isotp.c)
else()
    add_library(isotp SHARED ${CMAKE_CURRENT_SOURCE_DIR}/isotp.c)
endif()

###
# Strict building policies
###
if (MSVC)
    target_compile_options(
        isotp PRIVATE
        /W4
    )
else()
    target_compile_options(
        isotp PRIVATE
        -Werror
        -Wall
        -Wpedantic
        -Wextra
        -Wno-unknown-pragmas # ignore unknown pragmas, such as #pragma region
    )
endif()

###
# Enable position independent code (PIC) if required
###
if (NOT isotpc_STATIC_LIBRARY OR isotpc_STATIC_LIBRARY_PIC)
    # Building a shared library or a static library with PIC enabled
    target_compile_options(isotp PRIVATE -fPIC)
endif()

###
# Provide padding configuration
###
if (isotpc_PAD_CAN_FRAMES)
    target_compile_definitions(isotp PRIVATE -DISO_TP_FRAME_PADDING -DISO_TP_FRAME_PADDING_VALUE=${isotpc_CAN_FRAME_PAD_VALUE})
endif()

###
# Include additional arg in isotp_user_send_can if required
###
if (isotpc_ENABLE_CAN_SEND_ARG)
    target_compile_options(isotp PRIVATE -DISO_TP_USER_SEND_CAN_ARG)
endif()

###
# Add support for options added in #51
###
if (isotpc_ENABLE_TRANSCEIVE_EVENTS)
    message(STATUS "[isotp-c] Enabling ISOTP events; backwards compatibility may BREAK!")
    if (isotpc_ENABLE_TRANSMIT_COMPLETE_CALLBACK)
        target_compile_definitions(isotp PRIVATE -DISO_TP_TRANSMIT_COMPLETE_CALLBACK)
    endif()

    if (isotpc_ENABLE_RECEIVE_COMPLETE_CALLBACK)
        target_compile_definitions(isotp PRIVATE -DISO_TP_RECEIVE_COMPLETE_CALLBACK)
    endif()
endif()

###
# Check for debug builds
###
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(isotp PRIVATE -O0 -g) # don't optimise and add debugging symbols
else()
    target_compile_options(isotp PRIVATE -O2) # optimise code
    target_link_libraries(isotp PRIVATE -s) # strip the library
endif()

if (isotpc_USE_INCLUDE_DIR)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/isotp_c)
    
    file(GLOB HEADERS FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
    file(COPY ${HEADERS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/isotp_c/)

    target_include_directories(isotp PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)

else()
    target_include_directories(isotp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()

###
# Add library aliases
###
add_library(isotpc ALIAS isotp)
add_library(isotp_c ALIAS isotp)
add_library(simon_cahill::isotp ALIAS isotp)
add_library(simon_cahill::isotpc ALIAS isotp)
add_library(simon_cahill::isotp_c ALIAS isotp)

###
# Enable testing
###
# if (isotpc_ENABLE_TESTING)
#     enable_testing()
#     add_subdirectory(test)
# endif()
