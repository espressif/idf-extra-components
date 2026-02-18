idf_component_register(SRCS "spi_nand_flash_bdl_example_main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES spi_nand_flash spi_nand_flash_vfs
                       )

