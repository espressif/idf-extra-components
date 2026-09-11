set(BLEMESH_MANAGER_SRCS
    "src/blemesh_manager.c"
    "src/blemesh_prov_task.c"
    "src/blemesh_provisioner.c"
    "src/blemesh_configurator.c"
    "src/blemesh_node_directory.c"
    "src/blemesh_pending_confirm.c"
    "src/blemesh_heartbeat.c"
    "src/blemesh_initial_sync.c"
    "src/blemesh_publication_rx.c"
    "src/blemesh_dispatcher.c"
)
set(BLEMESH_MANAGER_INCLUDE_DIRS "include")
set(BLEMESH_MANAGER_PRIV_INCLUDE_DIRS "src")
set(BLEMESH_MANAGER_REQUIRES "bt")

idf_component_register(
    SRCS ${BLEMESH_MANAGER_SRCS}
    INCLUDE_DIRS ${BLEMESH_MANAGER_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${BLEMESH_MANAGER_PRIV_INCLUDE_DIRS}
    REQUIRES ${BLEMESH_MANAGER_REQUIRES}
)
