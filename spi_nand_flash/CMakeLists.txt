idf_build_get_property(target IDF_TARGET)

set(reqs fatfs)
set(inc diskio include)
set(priv_inc priv_include)
set(srcs "src/nand.c"
         "src/dhara_glue.c"
         "src/nand_impl_wrap.c"
         "diskio/diskio_nand.c")

if(${target} STREQUAL "linux")
    list(APPEND srcs "src/nand_impl_linux.c"
                 "src/nand_linux_mmap_emul.c")
else()
    list(APPEND srcs "src/nand_winbond.c"
                 "src/nand_gigadevice.c"
                 "src/nand_alliance.c"
                 "src/nand_micron.c"
                 "src/nand_zetta.c"
                 "src/nand_xtx.c"
                 "src/nand_impl.c"
                 "src/nand_impl_wrap.c"
                 "src/nand_diag_api.c"
                 "src/spi_nand_oper.c"
                 "vfs/vfs_fat_spinandflash.c")

    set(priv_reqs vfs)
    list(APPEND inc vfs)

    if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER "5.3")
        list(APPEND reqs esp_driver_spi)
    else()
        list(APPEND reqs driver)
    endif()
endif()


idf_component_register(SRCS ${srcs}
        INCLUDE_DIRS ${inc}
        PRIV_INCLUDE_DIRS ${priv_inc}
        REQUIRES ${reqs}
        PRIV_REQUIRES ${priv_reqs})
