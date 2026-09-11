idf_component_register()

# The component adds the upstream project as a subdirectory to the build tree
# But it does not install it to the system
set(FMT_INSTALL OFF)
add_subdirectory(fmt)

# The ESP C++ runtime only provides the "C" locale, and the path cannot be
# dropped at link time, so we always define FMT_USE_LOCALE=0.
# The definition has to be PUBLIC: it shapes the library sources, the public
# headers and the inline part of the library that is pulled into dependents.
target_compile_definitions(fmt PUBLIC FMT_USE_LOCALE=0)

# flockfile/funlockfile are implemented in ESP-IDF libc from v5.4.
# FMT_USE_FLOCKFILE=0 falls back to the buffered fwrite path.
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "5.4")
    target_compile_definitions(fmt PUBLIC FMT_USE_FLOCKFILE=0)
endif()

# Linking the target of the fmt subdirectory into the component library is what
# places the sources of fmt into the archive of the component and what exposes
# fmt::fmt to the dependents, exactly as before.
target_link_libraries(${COMPONENT_LIB} INTERFACE fmt)
