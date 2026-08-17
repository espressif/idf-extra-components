idf_component_register(SRCS "color-emoji-example.c"
                       INCLUDE_DIRS ".")

# Create a partition named "fonts" with the bundled CBDT/CBLC color font.
littlefs_create_partition_image(fonts ${CMAKE_CURRENT_LIST_DIR}/assets FLASH_IN_PROJECT)
