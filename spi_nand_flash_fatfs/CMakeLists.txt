set(srcs "src/diskio_nand.c"
         "src/vfs_fat_spinandflash.c")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    REQUIRES spi_nand_flash fatfs
    PRIV_REQUIRES vfs
)
