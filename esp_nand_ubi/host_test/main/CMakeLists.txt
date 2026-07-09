set(src "test_app_main.cpp"
        "test_nand_ubi.cpp")

idf_component_register(SRCS ${src}
                       WHOLE_ARCHIVE
                       )

target_link_libraries(${COMPONENT_LIB} PRIVATE Catch2WithMain)

# White-box tests reach into the component's private headers (media layout,
# CRC/validation helpers), which live in its src/ directory.
target_include_directories(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../src")
