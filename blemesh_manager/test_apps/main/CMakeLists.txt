idf_component_register(SRCS "blemesh_manager_test.c" "test_cfg_watchdog.c" "test_get_tracking.c"
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS "../../src"
                    REQUIRES blemesh_manager bt
                    PRIV_REQUIRES unity
                    WHOLE_ARCHIVE)

target_link_libraries(${COMPONENT_LIB} INTERFACE
                      "-Wl,--wrap=esp_ble_mesh_config_client_get_state"
                      "-Wl,--wrap=esp_ble_mesh_config_client_set_state"
                      "-Wl,--wrap=esp_ble_mesh_provisioner_delete_node_with_addr"
                      "-Wl,--wrap=esp_ble_mesh_generic_client_get_state"
                      "-Wl,--wrap=esp_ble_mesh_light_client_get_state")
