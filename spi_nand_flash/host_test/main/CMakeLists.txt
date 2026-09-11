set(src "test_app_main.cpp" "test_nand_ecc_decode.cpp")

if(CONFIG_NAND_FLASH_ENABLE_BDL)
    list(APPEND src "test_nand_flash_bdl.cpp")
else()
    list(APPEND src  "test_nand_flash.cpp" "test_nand_flash_ftl.cpp")
endif()

idf_component_register(SRCS ${src}
                       WHOLE_ARCHIVE
                       )

# ECC decode helpers are internal to spi_nand_flash; tests reach into its priv_include.
idf_component_get_property(nand_dir spi_nand_flash COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${nand_dir}/priv_include")

target_link_libraries(${COMPONENT_LIB} PRIVATE Catch2WithMain)
