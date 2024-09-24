
set(srcs coremark/core_list_join.c
         coremark/core_main.c
         coremark/core_matrix.c
         coremark/core_state.c
         coremark/core_util.c
         port/core_portme.c
)

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    configure_file(linker.lf.in ${CMAKE_CURRENT_BINARY_DIR}/linker.lf)
endif()

idf_component_register(SRCS ${srcs}
                       PRIV_INCLUDE_DIRS port coremark
                       LDFRAGMENTS ${CMAKE_CURRENT_BINARY_DIR}/linker.lf
                       PRIV_REQUIRES esp_timer)

# compile coremark component with -O3 flag (will override the optimization level which is set globally)
# set "-fjump-tables" and "-ftree-switch-conversion" explicitly, since IDF build system disables them by default
set(component_compile_options "-O3" "-fjump-tables" "-ftree-switch-conversion")
target_compile_options(${COMPONENT_LIB} PRIVATE ${component_compile_options})

# Get the compilation options and store them as a target property
set(compile_options_list "$<TARGET_PROPERTY:__idf_build_target,COMPILE_OPTIONS>;$<TARGET_PROPERTY:__idf_build_target,C_COMPILE_OPTIONS>;${component_compile_options}")
set_target_properties(${COMPONENT_LIB} PROPERTIES COMPILER_OPT "${compile_options_list}")

# Generate core_portme.h file, expanding "COMPILER_OPT" and "COMPILER_VER" generator expressions
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/core_portme.h INPUT ${CMAKE_CURRENT_SOURCE_DIR}/port/core_portme.h.in TARGET ${COMPONENT_LIB})
