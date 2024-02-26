set(srcs "src/nand.c"
         "src/spi_nand_oper.c"
         "src/dhara_glue.c"
         "vfs/vfs_fat_spinandflash.c"
         "diskio/diskio_nand.c")

idf_component_register(SRCS ${srcs}
        INCLUDE_DIRS include vfs diskio
        REQUIRES driver fatfs vfs
        )
