set(srcs)
set(priv_inc)

if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" GREATER_EQUAL "6.0" AND CONFIG_ESP_NAND_UBI_ENABLE)
    list(APPEND srcs "src/nand_ubi.c"
                     "src/nand_ubi_eba.c"
                     "src/nand_ubi_io.c")
    list(APPEND priv_inc "src")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS include
                       PRIV_INCLUDE_DIRS ${priv_inc}
                       REQUIRES esp_blockdev spi_nand_flash
                       PRIV_REQUIRES esp_rom)
