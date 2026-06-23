/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file blemesh_internal.h
 * @brief Internal types, queue messages, and module-private helpers shared by
 *        the blemesh_manager source files.
 *
 * Anything in this header is private to the component; consumers must not
 * include it.
 */

#ifndef __BLEMESH_INTERNAL_H__
#define __BLEMESH_INTERNAL_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_err.h"

#include "blemesh_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tunables — defaults overridable via Kconfig ***********************************/

#include "sdkconfig.h"

#define CONFIG_BLEMESH_MGR_PENDING_TABLE_SIZE (CONFIG_BLE_MESH_MAX_PROV_NODES * 2)

/* Queue message taxonomy ********************************************************/

typedef enum {
    BLEMESH_MSG_NONE = 0,

    /* Public-API origin */
    BLEMESH_MSG_COMMISSION_ENABLE,    /**< enable_commissioning */
    BLEMESH_MSG_COMMISSION_TIMEOUT,   /**< auto-close fired */
    BLEMESH_MSG_REMOVE_NODE,
    BLEMESH_MSG_SET_STATE,
    BLEMESH_MSG_GET_STATE,
    BLEMESH_MSG_VENDOR_SEND,
    BLEMESH_MSG_REQUEST_SYNC,         /**< blemesh_manager_request_sync */
    BLEMESH_MSG_START_SYNC,           /**< replay restored nodes + seed initial sync */
    BLEMESH_MSG_STOP,

    /* Stack-callback origin */
    BLEMESH_MSG_PROV_EVT,             /**< provisioning state machine event */
    BLEMESH_MSG_CFG_EVT,              /**< Config Client reply */
    BLEMESH_MSG_RX_STATUS,            /**< Client-model Status arrival */
    BLEMESH_MSG_RX_HEARTBEAT,         /**< Heartbeat arrival */

    /* Timer origin */
    BLEMESH_MSG_PENDING_TICK,         /**< pending-confirmation deadline */
    BLEMESH_MSG_HEARTBEAT_TICK,       /**< heartbeat tracker period */
    BLEMESH_MSG_SYNC_TICK,            /**< initial-state sync FIFO drain tick */
} blemesh_msg_kind_t;

/** Generic queue message — payload union sized to the largest variant. */
typedef struct {
    blemesh_msg_kind_t kind;
    union {
        struct {
            bool     enable;
            uint32_t window_seconds;
        } commission;
        struct {
            blemesh_addr_t addr;
        } remove_node;
        struct {
            blemesh_addr_t        addr;
            blemesh_state_value_t val;
            uint32_t              transition_ms;
        } set_state;
        struct {
            blemesh_addr_t     addr;
            blemesh_state_id_t state;
        } get_state;
        struct {
            blemesh_addr_t            addr;
            blemesh_device_profile_t  profile;
        } request_sync;
        struct {
            blemesh_addr_t addr;
            uint16_t       company_id;
            uint32_t       opcode;
            size_t         len;
            uint8_t       *data;       /**< malloc'd; freed by task */
        } vendor;
        struct {
            blemesh_addr_t addr;
            int            event;      /**< stack-defined event id */
            uint8_t        uuid[16];
        } prov;
        struct {
            blemesh_addr_t addr;
            uint32_t       opcode;
            int            status;
            uint16_t       elem_addr;  /**< MODEL_APP/PUB_STATUS echo; disambiguates stale replies */
            uint16_t       model_id;   /**< MODEL_APP/PUB_STATUS echo; disambiguates stale replies */
            size_t         len;        /**< for COMPOSITION_DATA_STATUS payload */
            uint8_t       *data;       /**< malloc'd; freed by task */
        } cfg;
        struct {
            blemesh_addr_t addr;
            uint32_t       opcode;
            size_t         len;
            uint8_t       *data;       /**< malloc'd; freed by task */
        } rx_status;
        struct {
            blemesh_addr_t addr;
            uint8_t        rssi;
        } heartbeat;
    } u;
} blemesh_msg_t;

/* Node directory entry **********************************************************/

typedef struct {
    bool                used;
    blemesh_node_info_t info;            /**< info.reachable is the live flag */
    uint8_t             missed_heartbeats;
    uint32_t            provisioned_ts_ms;
} blemesh_node_entry_t;

/* Pending-confirmation entry ****************************************************/

typedef enum {
    BLEMESH_PENDING_KIND_SET = 0,
    BLEMESH_PENDING_KIND_GET,
} blemesh_pending_kind_t;

typedef struct {
    bool                   used;
    blemesh_addr_t         addr;
    blemesh_state_id_t     state_id;
    uint64_t               deadline_ms;
    uint8_t                retries_left;
    blemesh_pending_kind_t kind;
    blemesh_state_value_t  last_value;   /**< last requested value (for retry / rollback context) */
} blemesh_pending_entry_t;

/* Module-shared context *********************************************************/

typedef struct {
    bool                                  initialized;
    bool                                  started;
    bool                                  commissioning_open;  /**< true between enable_commissioning(true) and (false) */
    volatile bool                         provisioning_busy;   /**< true from add_unprov_dev until configurator finishes */
    blemesh_manager_config_t              cfg;
    blemesh_manager_callbacks_t           cb;
    blemesh_manager_models_t              models;

    TaskHandle_t                          task;
    QueueHandle_t                         queue;

    /* FreeRTOS software timers */
    TimerHandle_t                         commission_timer;   /**< one-shot */
    TimerHandle_t                         pending_timer;      /**< one-shot, re-armed */
    TimerHandle_t                         heartbeat_timer;    /**< auto-reload */
    TimerHandle_t                         sync_timer;         /**< one-shot, re-armed; paces initial-state Gets */

    /* Node directory (volatile) */
    blemesh_node_entry_t                  nodes[CONFIG_BLEMESH_MGR_MAX_NODES];

    /* Pending confirmations */
    blemesh_pending_entry_t               pending[CONFIG_BLEMESH_MGR_PENDING_TABLE_SIZE];
} blemesh_ctx_t;

extern blemesh_ctx_t g_blemesh_ctx;

/* Helpers ***********************************************************************/

/** Post a message to the manager task. Non-blocking; returns ESP_FAIL on full queue. */
esp_err_t blemesh_post(const blemesh_msg_t *msg);

/**
 * Free the heap payload a message carries, if any. Single source of truth for
 * the msg-kind -> owning union member mapping. Call on a message that never
 * reached the task (blemesh_post failed), and on the task side once a message
 * is consumed. Safe on messages with no payload.
 */
void blemesh_free_msg(const blemesh_msg_t *msg);

/**
 * @brief SIG server model id whose presence makes an element a standalone
 *        logical element for the given profile, or 0 when the profile maps to
 *        a single logical element at the primary element regardless of layout
 *        (vendor/unknown, or level-only dimmables).
 *
 * Composite-light secondary elements carry *different* model ids (CTL
 * Temperature, HSL Hue/Saturation), so matching on the primary model id alone
 * naturally excludes channel elements. See blemesh_node_info_t.logical_elem_offset.
 */
uint16_t blemesh_primary_model_for_profile(blemesh_device_profile_t profile);

/**
 * @brief Derive the logical-element offset list from raw Composition Data
 *        Page 0 by collecting elements that bear the profile's primary model.
 *
 * Falls back to a single logical element at offset 0 when the primary model is
 * not found (or the profile has none). @p offsets must hold at least
 * BLEMESH_MAX_LOGICAL_ELEMS_PER_NODE entries; @p count is set to >= 1.
 */
void blemesh_comp_logical_elem_offsets(const uint8_t *comp_data, size_t comp_len,
                                       blemesh_device_profile_t profile,
                                       uint8_t *offsets, uint8_t *count);

/**
 * @brief Element offset of the Light CTL Temperature Server in raw Composition
 *        Data Page 0, or 0 if the node has none. See
 *        blemesh_node_info_t.ctl_temp_offset.
 */
uint8_t blemesh_comp_ctl_temp_offset(const uint8_t *comp_data, size_t comp_len);

/**
 * @brief Element offsets of the Light HSL Hue / Saturation Servers in raw
 *        Composition Data Page 0, each 0 if absent. See
 *        blemesh_node_info_t.hsl_hue_offset / hsl_sat_offset.
 */
void blemesh_comp_hsl_offsets(const uint8_t *comp_data, size_t comp_len,
                              uint8_t *hue_off, uint8_t *sat_off);

/** Current monotonic time in milliseconds. */
uint64_t blemesh_now_ms(void);

/* Subsystem entry points (called only from the task) ****************************/

void blemesh_task_main(void *arg);
void blemesh_handle_message(const blemesh_msg_t *msg);

/* Node directory */
void blemesh_dir_clear(void);
blemesh_node_entry_t *blemesh_dir_find(blemesh_addr_t addr);
/** Resolve any element address to its owning node (primary <= addr < primary+element_count). */
blemesh_node_entry_t *blemesh_dir_find_by_element(blemesh_addr_t addr);
blemesh_node_entry_t *blemesh_dir_insert(const blemesh_node_info_t *info);
esp_err_t blemesh_dir_remove(blemesh_addr_t addr);
size_t blemesh_dir_count(void);
void blemesh_dir_snapshot(blemesh_node_info_t *out, size_t *count);

/* Pending confirmation */
void blemesh_pending_init(void);
void blemesh_pending_upsert(blemesh_addr_t addr,
                            blemesh_state_id_t state_id,
                            const blemesh_state_value_t *val,
                            uint64_t timeout_ms,
                            uint8_t retries);
void blemesh_pending_clear(blemesh_addr_t addr, blemesh_state_id_t state_id);
void blemesh_pending_clear_all_for(blemesh_addr_t addr);
void blemesh_pending_on_tick(void);
void blemesh_pending_rearm(void);

/* Heartbeat tracker */
void blemesh_heartbeat_init(void);
void blemesh_heartbeat_start(void);
void blemesh_heartbeat_stop(void);
void blemesh_heartbeat_on_packet(blemesh_addr_t addr);
void blemesh_heartbeat_on_tick(void);

/* Initial-state sync — paced Gets after a node is provisioned or restored */
void blemesh_initial_sync_init(void);
void blemesh_initial_sync_enqueue(blemesh_addr_t addr,
                                  blemesh_device_profile_t profile);
void blemesh_initial_sync_on_tick(void);
void blemesh_initial_sync_clear_for(blemesh_addr_t addr);
void blemesh_initial_sync_clear_all(void);

/* Provisioning state machine */
esp_err_t blemesh_provisioner_init(void);
/** Post-`esp_ble_mesh_init` hook: enables heartbeat reception. Must be
 *  called only after `esp_ble_mesh_init` has brought up the BTC task. */
esp_err_t blemesh_provisioner_enable_heartbeat_rx(void);
esp_err_t blemesh_provisioner_set_enabled(bool enable);
void blemesh_provisioner_on_event(const blemesh_msg_t *msg);

/* Configurator */
void blemesh_configurator_start(blemesh_addr_t addr, const uint8_t uuid[16]);
void blemesh_configurator_on_reply(const blemesh_msg_t *msg);
bool blemesh_configurator_busy(void);

/* Publication RX */
esp_err_t blemesh_publication_rx_init(void);
void blemesh_publication_rx_dispatch(const blemesh_msg_t *msg);

/* Outbound dispatcher */
esp_err_t blemesh_dispatcher_init(void);
esp_err_t blemesh_dispatcher_send_set(blemesh_addr_t addr,
                                      const blemesh_state_value_t *val,
                                      uint32_t transition_ms);
esp_err_t blemesh_dispatcher_send_get(blemesh_addr_t addr,
                                      blemesh_state_id_t state_id);
esp_err_t blemesh_dispatcher_send_vendor(blemesh_addr_t addr,
        uint16_t company_id,
        uint32_t opcode,
        const uint8_t *data,
        size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __BLEMESH_INTERNAL_H__ */
