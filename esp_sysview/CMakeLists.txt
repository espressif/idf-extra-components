if(CONFIG_ESP_TRACE_LIB_EXTERNAL)
    set(include_dirs "")
    set(srcs "")

    list(APPEND include_dirs
        src/Config
        src/SEGGER
        src/Sample/FreeRTOSV10.4
        src/include)

    list(APPEND srcs
        src/SEGGER/SEGGER_SYSVIEW.c
        src/Sample/FreeRTOSV10.4/Config/esp/SEGGER_SYSVIEW_Config_FreeRTOS.c
        src/Sample/FreeRTOSV10.4/SEGGER_SYSVIEW_FreeRTOS.c
        src/esp/SEGGER_RTT_esp.c
        src/esp/SEGGER_SYSVIEW_esp.c
        src/esp/adapter_encoder_sysview.c
        src/ext/logging.c)

    if(CONFIG_HEAP_TRACING_TOHOST)
        list(APPEND srcs
            src/ext/heap_trace_module.c
            src/ext/heap_trace_tohost.c)
        if(CONFIG_IDF_TARGET_ARCH_XTENSA)
            set_source_files_properties(src/ext/heap_trace_tohost.c PROPERTIES COMPILE_FLAGS -Wno-frame-address)
        endif()
    endif()

    idf_component_register(SRCS "${srcs}"
                        INCLUDE_DIRS "${include_dirs}"
                        REQUIRES     esp_trace
                        LDFRAGMENTS linker.lf
                        WHOLE_ARCHIVE TRUE # Link all symbols so self-registering adapters (ESP_TRACE_REGISTER_*) are not discarded
                        )

    # Trick to include the SEGGER header into freertos component
    # This allows freertos component to find esp_trace_freertos_impl.h
    idf_component_get_property(freertos_lib freertos COMPONENT_LIB)
    target_include_directories(${freertos_lib} INTERFACE ${include_dirs})
else()
    idf_component_register(REQUIRES "esp_trace")
endif()
