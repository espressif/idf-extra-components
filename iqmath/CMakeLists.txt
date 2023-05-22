set(srcs
    "_IQNfunctions/_atoIQN.c"
    "_IQNfunctions/_IQNasin_acos.c"
    "_IQNfunctions/_IQNatan2.c"
    "_IQNfunctions/_IQNdiv.c"
    "_IQNfunctions/_IQNexp.c"
    "_IQNfunctions/_IQNfrac.c"
    "_IQNfunctions/_IQNlog.c"
    "_IQNfunctions/_IQNmpy.c"
    "_IQNfunctions/_IQNmpyIQX.c"
    "_IQNfunctions/_IQNrepeat.c"
    "_IQNfunctions/_IQNrmpy.c"
    "_IQNfunctions/_IQNrsmpy.c"
    "_IQNfunctions/_IQNsin_cos.c"
    "_IQNfunctions/_IQNsqrt.c"
    "_IQNfunctions/_IQNtables.c"
    "_IQNfunctions/_IQNtoa.c"
    "_IQNfunctions/_IQNtoF.c"
    "_IQNfunctions/_IQNversion.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include")
