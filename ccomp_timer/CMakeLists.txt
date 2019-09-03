set(srcs "ccomp_timer.c")

if(CONFIG_IDF_TARGET_ARCH_RISCV)
    list(APPEND srcs "ccomp_timer_impl_riscv.c")
endif()

if(CONFIG_IDF_TARGET_ARCH_XTENSA)
    list(APPEND srcs "ccomp_timer_impl_xtensa.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS include
                       PRIV_INCLUDE_DIRS private_include
                       PRIV_REQUIRES perfmon driver)
