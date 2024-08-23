# ThorVG uses meson as its build system, check if it's installed
find_program(MESON_EXECUTABLE meson)
if(NOT MESON_EXECUTABLE)
    message(STATUS "Meson build system not found. Attempting to install it using pip...")

    # Try to install meson using pip
    idf_build_get_property(python PYTHON)
    execute_process(
        COMMAND ${python} -m pip install -U meson
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )

    if(result)
        message(FATAL_ERROR "Failed to install meson using pip. Please install it manually.\nError: ${error}")
    else()
        message(STATUS "Meson successfully installed.")
    endif()
else()
    message(STATUS "Meson build system found: ${MESON_EXECUTABLE}")
endif()
