idf_component_register(SRCS "freetype-example.c"
                       INCLUDE_DIRS ".")

# Download the example font into a directory "littlefs" in the build directory
set(URL "https://github.com/espressif/esp-docs/raw/f036a337d8bee5d1a93b2264ecd29255baec4260/src/esp_docs/fonts/DejaVuSans.ttf")
set(FILE "DejaVuSans.ttf")
set(LITTLEFS_DIR "${CMAKE_BINARY_DIR}/littlefs")
file(MAKE_DIRECTORY ${LITTLEFS_DIR})
file(DOWNLOAD ${URL} ${LITTLEFS_DIR}/${FILE} SHOW_PROGRESS)

# Create a partition named "fonts" with the example font
littlefs_create_partition_image(fonts ${LITTLEFS_DIR} FLASH_IN_PROJECT)
