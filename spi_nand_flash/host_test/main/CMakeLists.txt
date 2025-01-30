idf_component_register(SRCS "test_nand_flash.cpp" "test_app_main.cpp"
                       WHOLE_ARCHIVE
                       )

target_link_libraries(${COMPONENT_LIB} PRIVATE Catch2WithMain)
