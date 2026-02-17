set(srcs "src/diskio_nand.c"
         "src/vfs_fat_spinandflash.c")

# Conditionally add BDL diskio support
if(CONFIG_NAND_FLASH_ENABLE_BDL)
    list(APPEND srcs "src/diskio_nand_blockdev.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    REQUIRES spi_nand_flash fatfs
    PRIV_REQUIRES vfs
)
