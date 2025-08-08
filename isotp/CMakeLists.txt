# Apply patch to isotp-c submodule
find_package(Git REQUIRED)

execute_process(
    COMMAND ${GIT_EXECUTABLE} -C ${CMAKE_CURRENT_SOURCE_DIR}/isotp-c apply --ignore-whitespace ${CMAKE_CURRENT_SOURCE_DIR}/patch/isotp-c.patch
    RESULT_VARIABLE GIT_PATCH_RESULT
)

if(NOT GIT_PATCH_RESULT EQUAL "0")
    message(WARNING "Failed to apply isotp-c patch! It's possible that the patch is already applied.")
endif()

idf_component_register(
    SRCS "esp_isotp/esp_isotp.c" "isotp-c/isotp.c"
    INCLUDE_DIRS "esp_isotp" "." "isotp-c"
    REQUIRES driver esp_timer esp_driver_twai
)
