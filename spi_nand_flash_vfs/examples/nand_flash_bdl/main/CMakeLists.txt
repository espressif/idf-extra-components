if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "6.0")
    message(FATAL_ERROR "Block Device Layer (BDL) support requires ESP-IDF >= 6.0. "
         "Please upgrade your ESP-IDF version to run this example.")
endif()

idf_component_register(SRCS "spi_nand_flash_bdl_example_main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES spi_nand_flash spi_nand_flash_vfs
                       )

