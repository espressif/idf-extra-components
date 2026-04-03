set(srcs "src/pid_ctrl.c")
set(requires "iqmath")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "include"
                       REQUIRES ${requires})
