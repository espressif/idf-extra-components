set(srcs "test_app_main.c"
         "test_esp_flash_dispatcher.c")

# In order for the cases defined by `TEST_CASE` to be linked into the final elf,
# the component can be registered as WHOLE_ARCHIVE
idf_component_register(SRCS ${srcs}
                       PRIV_REQUIRES unity spi_flash esp_psram esp_partition esp_flash_dispatcher
                       INCLUDE_DIRS "."
                       WHOLE_ARCHIVE)
