set(srcs "src/nand.c"
         "src/spi_nand_oper.c"
         "src/dhara_glue.c"
         "vfs/vfs_fat_spinandflash.c"
         "diskio/diskio_nand.c")

set(reqs fatfs)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER "5.3")
    list(APPEND reqs esp_driver_spi)
else()
    list(APPEND reqs driver)
endif()

set(priv_reqs vfs)

idf_component_register(SRCS ${srcs}
        INCLUDE_DIRS include vfs diskio
        REQUIRES ${reqs}
        PRIV_REQUIRES ${priv_reqs})
