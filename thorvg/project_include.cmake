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
        # The first lookup is cached as MESON_EXECUTABLE-NOTFOUND. Clear the
        # cache and search again now that pip has created the PATH-visible
        # launcher in the ESP-IDF Python environment.
        unset(MESON_EXECUTABLE CACHE)
        unset(MESON_EXECUTABLE)
        find_program(MESON_EXECUTABLE meson)
        if(NOT MESON_EXECUTABLE)
            message(FATAL_ERROR "Meson was installed but its launcher could not be found on PATH.")
        endif()
        message(STATUS "Meson successfully installed: ${MESON_EXECUTABLE}")
    endif()
else()
    message(STATUS "Meson build system found: ${MESON_EXECUTABLE}")
endif()
