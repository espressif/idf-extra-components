idf_build_get_property(arch IDF_TARGET_ARCH)

set(srcs "ccomp_timer.c")

if(CONFIG_IDF_TARGET_ARCH_RISCV)
    list(APPEND srcs "ccomp_timer_impl_riscv.c")
endif()

if(CONFIG_IDF_TARGET_ARCH_XTENSA)
    list(APPEND srcs "ccomp_timer_impl_xtensa.c")
endif()

if("${arch}" STREQUAL "xtensa")
    set(priv_requires perfmon driver)
else()
    set(priv_requires driver)
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS include
                       PRIV_INCLUDE_DIRS private_include
                       PRIV_REQUIRES "${priv_requires}")
