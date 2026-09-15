set(srcs "src/clarke_park_f.c" "src/clarke_park_iq.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include")
