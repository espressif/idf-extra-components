idf_component_register()

# Override options defined in freetype/CMakeLists.txt.
# We could have used normal set(...) here if freetype enabled CMake policy CMP0077.
option(FT_DISABLE_HARFBUZZ "" ON)
option(FT_DISABLE_BZIP2 "" ON)
option(FT_DISABLE_BROTLI "" ON)
option(FT_DISABLE_HVF "" ON)

# FT_DISABLE_STREAM_SUPPORT stays at the upstream default (OFF): examples and
# typical IDF apps load fonts from LittleFS/FAT via FT_New_Face.

# These are regular CMake variables, so we can set them directly.
set(SKIP_INSTALL_ALL TRUE)
set(BUILD_SHARED_LIBS OFF)

add_subdirectory(freetype)

target_link_libraries(${COMPONENT_LIB} INTERFACE freetype)
