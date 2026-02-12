# Enable Swift support early, before component registration
# Force Whole Module builds (required by Embedded Swift)
# Use CMAKE_Swift_COMPILER_WORKS to skip trial compilations which don't work when cross-compiling
set(CMAKE_Swift_COMPILER_WORKS YES)
set(CMAKE_Swift_COMPILATION_MODE_DEFAULT wholemodule)
set(CMAKE_Swift_COMPILATION_MODE wholemodule)
enable_language(Swift)

function(force_cxx_linker)
    # Get the executable target name from ESP-IDF build properties
    idf_build_get_property(project_elf EXECUTABLE)

    if(TARGET ${project_elf})
        set_target_properties(${project_elf} PROPERTIES LINKER_LANGUAGE CXX)
        message(STATUS "Swift: Forced ${project_elf} to use CXX linker")
    else()
        message(FATAL_ERROR "Swift: Cannot set linker language - executable target not found")
    endif()
endfunction()

# Automatically defer the call to force CXX linker after all targets are created
# This ensures the final executable uses CXX linker instead of swiftc
cmake_language(DEFER CALL force_cxx_linker)
