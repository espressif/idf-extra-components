set(srcs "src/diskio_nand.c"
         "src/vfs_fat_spinandflash.c")

# esp_vfs_fat_bdl_* / diskio_bdl.h exist from IDF 6.1+ (NAND BDL itself is IDF 6.0+)
if(CONFIG_NAND_FLASH_ENABLE_BDL AND "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "6.1")
    list(APPEND srcs "src/vfs_fat_nand_bdl.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    REQUIRES spi_nand_flash fatfs
    PRIV_REQUIRES vfs
)
