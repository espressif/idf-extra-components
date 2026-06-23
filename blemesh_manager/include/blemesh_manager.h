/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file blemesh_manager.h
 * @brief State-oriented BLE Mesh provisioner / operator facade.
 *
 * Exposes a node-and-state API ("set OnOff on node 0x0007 to true") on top of
 * the ESP-BLE-MESH stack. Hides opcodes, AppKeys, model IDs, retransmits,
 * publication setup, and heartbeat tracking from the consumer.
 *
 * Threading: public APIs that interact with the mesh (e.g. commissioning, set/get/remove, sync, start/stop)
 * enqueue work onto the manager's internal task. Callbacks are invoked only from that single task,
 * so callbacks MUST NOT block. APIs that explicitly run inline (e.g. init/deinit/reset and registration helpers) are not callback-context-safe.
 *
 * Persistence: the manager's in-memory directory is volatile, but the underlying
 * ESP-BLE-MESH stack can persist keys and the provisioned-node table when
 * CONFIG_BLE_MESH_SETTINGS=y. On boot, blemesh_manager_init() rebuilds the
 * directory from the stack's node table and blemesh_manager_start() replays
 * restored nodes via on_node_provisioned(..., is_fresh=false).
 */

#ifndef __BLEMESH_MANAGER_H__
#define __BLEMESH_MANAGER_H__

/* Includes **********************************************************************/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_ble_mesh_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Types ************************************************************************/

/** Mesh unicast address (16 bits, per SIG Mesh Profile). */
typedef uint16_t blemesh_addr_t;

/**
 * @brief Max distinct logical elements a single mesh node maps to.
 *
 * A node maps to one logical element per *independent* element (e.g. each gang
 * of a multi-gang OnOff switch). Composite single devices (CTL, HSL) collapse
 * their spec-mandated channel elements into a single logical element, so this
 * only needs to be large for genuinely multi-device nodes.
 */
#define BLEMESH_MAX_LOGICAL_ELEMS_PER_NODE 16

/** Coarse classification of a provisioned node based on its Composition Data. */
typedef enum {
    BLEMESH_DEV_UNKNOWN = 0,
    BLEMESH_DEV_ONOFF,          /**< Generic OnOff Server */
    BLEMESH_DEV_DIMMABLE,       /**< + Generic Level / Light Lightness */
    BLEMESH_DEV_COLOR_TEMP,     /**< + Light CTL */
    BLEMESH_DEV_COLOR_HSL,      /**< + Light HSL */
    BLEMESH_DEV_OCCUPANCY,      /**< Sensor Server, motion property */
    BLEMESH_DEV_TEMPERATURE,    /**< Sensor Server, temperature property */
    BLEMESH_DEV_VENDOR,         /**< Unknown / vendor — passthrough */
} blemesh_device_profile_t;

/** State family identifiers exposed by the manager. */
typedef enum {
    BLEMESH_STATE_ONOFF,
    BLEMESH_STATE_LEVEL,
    BLEMESH_STATE_LIGHTNESS,
    BLEMESH_STATE_CTL,
    BLEMESH_STATE_HSL,
    BLEMESH_STATE_HSL_HUE,   /**< hue-only change/report (Light HSL Hue element) */
    BLEMESH_STATE_HSL_SAT,   /**< saturation-only change/report (Light HSL Saturation element) */
    BLEMESH_STATE_SENSOR_VALUE,
} blemesh_state_id_t;

/**
 * @brief Sentinel for a CTL/HSL lightness field that carries no new value.
 *
 * A Light CTL Temperature Status (from the Temperature element) reports only
 * temperature, with no lightness; likewise an HSL Hue/Saturation Status. The
 * dispatcher sets the absent lightness field to this value so the consumer can
 * update just the meaningful channel and leave brightness untouched. 0xFFFF is
 * reserved by the manager as an "unchanged" sentinel; if a device reports a
 * real lightness of 0xFFFF, the RX path remaps it to 0xFFFE to avoid ambiguity.
 */
#define BLEMESH_LIGHTNESS_UNCHANGED 0xFFFFu

/** Tagged value carrying any state. */
typedef struct {
    blemesh_state_id_t id;
    union {
        bool     onoff;
        int16_t  level;
        uint16_t lightness;
        struct {
            uint16_t l, h, s;
        } hsl;
        struct {
            uint16_t lightness, temperature;
        } ctl;
        struct {
            uint16_t property_id;
            uint8_t  len;
            uint8_t  data[16];
        } sensor;
    } v;
} blemesh_state_value_t;

/** Volatile descriptor of a provisioned node. */
typedef struct {
    blemesh_addr_t           addr;
    blemesh_device_profile_t profile;
    uint8_t                  element_count;
    uint16_t                 company_id;      /**< 0xFFFF when SIG-only */
    char                     uuid[37];        /**< canonical UUID string */
    bool                     reachable;       /**< current heartbeat status */

    /**
     * @brief Element offsets that each map to a distinct logical element.
     *
     * A BLE-Mesh node has one or more elements, but not every element is its
     * own device: composite models force extra elements that are *channels* of
     * one device (e.g. Light HSL Server on the primary element mandates trailing
     * Hue and Saturation elements; Light CTL mandates a Temperature element).
     * Those channel elements must NOT each be exposed as a separate logical
     * element.
     *
     * The standard exposes no grouping flag, so the manager derives this from
     * the per-element model layout: an element is a logical element iff it
     * carries the profile's *primary* server model (so HSL/CTL collapse to one
     * logical element, while a multi-gang OnOff with N elements each bearing
     * Generic OnOff Server yields N logical elements). Always >= 1; entry [0] is
     * the primary element.
     *
     * Logical element i lives at unicast address `addr + logical_elem_offset[i]`.
     */
    uint8_t                  logical_elem_count;
    uint8_t                  logical_elem_offset[BLEMESH_MAX_LOGICAL_ELEMS_PER_NODE];

    /**
     * @brief Element offset of the node's Light CTL Temperature Server, or 0
     *        when the node has none.
     *
     * A Light CTL device is spec-mandated to carry its Light CTL Temperature
     * Server on a *separate* (secondary) element from the main Light CTL
     * Server. Changing color temperature without disturbing brightness
     * requires a Light CTL Temperature Set sent to that element
     * (`addr + ctl_temp_offset`) — the combined Light CTL Set on the main
     * element always re-asserts a lightness value. 0 is never a valid value
     * for a real temperature element (offset 0 is the primary element), so it
     * doubles as "absent".
     */
    uint8_t                  ctl_temp_offset;

    /**
     * @brief Element offsets of the Light HSL Hue / Saturation Servers, or 0
     *        when the node has none.
     *
     * Like Light CTL, a Light HSL device carries its Hue and Saturation Servers
     * on separate secondary elements. Changing hue or saturation without
     * disturbing the others requires a Light HSL Hue Set / Saturation Set to the
     * matching element (`addr + hsl_hue_offset` / `addr + hsl_sat_offset`); the
     * combined Light HSL Set re-asserts all three channels. 0 = absent (offset 0
     * is the primary element).
     */
    uint8_t                  hsl_hue_offset;
    uint8_t                  hsl_sat_offset;
} blemesh_node_info_t;

/** Static configuration supplied at init. */
typedef struct {
    /**
     * @brief Provisioner-side provisioning parameters. Caller owns the
     *        struct; manager passes it verbatim to esp_ble_mesh_init().
     *        Required.
     */
    esp_ble_mesh_prov_t *prov;

    /**
     * @brief Provisioner composition: elements + models declared by the
     *        application. Required. Must contain Config Server + Config
     *        Client and any Client models you wish to drive via the manager.
     */
    esp_ble_mesh_comp_t *comp;

    uint8_t  net_key[16];               /**< Initial network key. */
    uint8_t  app_key[16];               /**< AppKey that the configurator binds to nodes. */
    uint16_t prov_addr_start;           /**< Provisioner's own unicast address range start. */
    uint16_t prov_addr_count;           /**< Number of unicast addresses owned by the provisioner. */
    uint16_t uplink_group_addr;         /**< Group address nodes publish to (e.g. 0xC000). */
    uint32_t heartbeat_period_ms;       /**< Configured period for node heartbeats. Default 30000. */
    uint8_t  heartbeat_loss_threshold;  /**< Missed periods before a node is marked unreachable. Default 3. */
    uint32_t set_confirm_timeout_ms;    /**< Deadline for arrival of confirming Status. Default 4000. */
    uint8_t  set_retries;               /**< Set re-sends after a Set deadline elapses (before escalating to a Get). Default 1. */
    uint8_t  default_ttl;               /**< Default TTL to configure on each node. Typical 5–7; default 7. */
} blemesh_manager_config_t;

/** Asynchronous event callbacks. Invoked from the manager task only. */
typedef struct {
    /**
     * @param is_fresh true: node just completed provisioning (assume online);
     *                 false: node replayed from NVS on boot (reachability
     *                 unknown until the first heartbeat).
     */
    void (*on_node_provisioned)(const blemesh_node_info_t *info, bool is_fresh);
    void (*on_node_removed)(blemesh_addr_t addr);
    void (*on_reachability)(blemesh_addr_t addr, bool reachable);
    void (*on_state_changed)(blemesh_addr_t addr, const blemesh_state_value_t *val);
    void (*on_set_failed)(blemesh_addr_t addr, blemesh_state_id_t state);
    void (*on_vendor_message)(blemesh_addr_t addr,
                              uint16_t company_id,
                              uint32_t opcode,
                              const uint8_t *data,
                              size_t len);
} blemesh_manager_callbacks_t;

/* Lifecycle *********************************************************************/

/**
 * @brief Bring up everything ESP-BLE-MESH-related the manager needs.
 *
 * The caller must already have initialised NVS, the BT controller, and the
 * Bluedroid host (since those resources are typically shared with other
 * components). The manager only owns the mesh layer.
 *
 * Performs, in order:
 *   1. registration of all ESP-BLE-MESH stack callbacks (provisioning,
 *      Config Client, Generic/Lighting/Sensor Client),
 *   2. esp_ble_mesh_init() with the caller-supplied prov + comp,
 *   3. esp_ble_mesh_provisioner_prov_enable() (so the primary NetKey
 *      materializes). Commissioning beacon handling is gated by
 *      blemesh_manager_enable_commissioning(), not prov_disable().
 *   4. esp_ble_mesh_provisioner_add_local_app_key(),
 *   5. spawn of the internal manager task and its FreeRTOS resources.
 *
 * After this returns ESP_OK the caller has only to register callbacks
 * + models and call blemesh_manager_start(). The caller should NOT call
 * esp_ble_mesh_init itself.
 *
 * Idempotent guard: returns ESP_ERR_INVALID_STATE if already initialized.
 */
esp_err_t blemesh_manager_init(const blemesh_manager_config_t *cfg);

/** Tear down the manager. Stops the task, cancels timers, releases resources. */
esp_err_t blemesh_manager_deinit(void);

/** Begin operation: enable mesh, subscribe uplink models to the uplink group. */
esp_err_t blemesh_manager_start(void);

/** Stop operation (mesh suspended; manager state retained). */
esp_err_t blemesh_manager_stop(void);

/**
 * @brief Wipe the ESP-BLE-MESH NVS namespace (net keys, IV index, node table,
 *        sequence number, etc.) and clear the in-memory directory. The
 *        manager is left de-initialized — callers must reboot
 *        (`esp_restart()`) and re-initialize from a clean slate.
 *
 * @note This call does NOT reboot the device. That is intentional: the
 *       caller is expected to drive its own composite-reset flow (e.g. wipe
 *       another subsystem's NVS first, then reboot). The standalone REPL
 *       example calls `esp_restart()` immediately after this function.
 */
esp_err_t blemesh_manager_reset(void);

/* Callback registration *********************************************************/

esp_err_t blemesh_manager_register_callbacks(const blemesh_manager_callbacks_t *cb);

/* Client-model registration *****************************************************/

/**
 * @brief Pointers to the consumer's Client model instances. The consumer
 *        builds its own provisioner composition (with the appropriate Client
 *        model structs) and hands the pointers in here. Any field may be NULL
 *        — operations targeting an unregistered state family return
 *        ESP_ERR_INVALID_STATE.
 *
 * The pointers must remain valid for the lifetime of the manager.
 */
typedef struct {
    esp_ble_mesh_model_t *config_client;
    esp_ble_mesh_model_t *generic_onoff_client;
    esp_ble_mesh_model_t *generic_level_client;
    esp_ble_mesh_model_t *light_lightness_client;
    esp_ble_mesh_model_t *light_ctl_client;
    esp_ble_mesh_model_t *light_hsl_client;
    esp_ble_mesh_model_t *sensor_client;
    esp_ble_mesh_model_t *vendor_client;     /**< optional — used by send_vendor */
} blemesh_manager_models_t;

/**
 * Stores the model pointers and binds the local AppKey to every non-NULL
 * Client model so they can encrypt outgoing messages. Call this once
 * between blemesh_manager_init() and blemesh_manager_start().
 */
esp_err_t blemesh_manager_register_models(const blemesh_manager_models_t *models);

/* Commissioning *****************************************************************/

/**
 * @brief Open/close the commissioning window.
 * @param enable          true to start scanning for unprovisioned beacons.
 * @param window_seconds  Auto-close window (0 = no auto-close).
 */
esp_err_t blemesh_manager_enable_commissioning(bool enable, uint32_t window_seconds);

/** Remove a node from the network and the directory. */
esp_err_t blemesh_manager_remove_node(blemesh_addr_t addr);

/**
 * @brief Snapshot the volatile directory.
 * @param[out]    out    Caller-owned buffer of capacity `*count`. May be NULL to query size.
 * @param[in,out] count  In: capacity. Out: number of entries written or required.
 */
esp_err_t blemesh_manager_list_nodes(blemesh_node_info_t *out, size_t *count);

/** Look up a single node by unicast address. */
esp_err_t blemesh_manager_get_node_info(blemesh_addr_t addr, blemesh_node_info_t *out);

/* Operations ********************************************************************/

/**
 * @brief Fire-and-forget Set. Result delivered via on_state_changed
 *        (success) or on_set_failed (escalation exhausted).
 * @param addr           Target node unicast address.
 * @param val            New state value.
 * @param transition_ms  Transition time hint for the model (0 = instant).
 */
esp_err_t blemesh_manager_set_state(blemesh_addr_t addr,
                                    const blemesh_state_value_t *val,
                                    uint32_t transition_ms);

/** Explicit Get. Result delivered via on_state_changed. */
esp_err_t blemesh_manager_get_state(blemesh_addr_t addr, blemesh_state_id_t state);

/** Vendor escape hatch: send an opaque message. */
esp_err_t blemesh_manager_send_vendor(blemesh_addr_t addr,
                                      uint16_t company_id,
                                      uint32_t opcode,
                                      const uint8_t *data,
                                      size_t len);

/**
 * @brief Request a paced initial-state sync for a single node element.
 *
 * Equivalent to calling blemesh_initial_sync_enqueue() internally.
 * Safe to call from any task; the actual Get
 * is dispatched through the manager task via a FreeRTOS timer.
 * Deduplicates: if the same (addr, state_id) is already queued this
 * is a no-op.
 */
esp_err_t blemesh_manager_request_sync(blemesh_addr_t addr,
                                       blemesh_device_profile_t profile);

#ifdef __cplusplus
}
#endif

#endif /* __BLEMESH_MANAGER_H__ */
