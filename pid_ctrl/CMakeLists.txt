set(srcs "src/pid_ctrl_f.c" "src/pid_ctrl_iq.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "include")
