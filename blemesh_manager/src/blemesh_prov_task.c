/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file blemesh_prov_task.c
 * @brief Single-threaded task loop. All public-API messages, ESP-BLE-MESH
 *        stack callbacks, and scheduler ticks funnel through one queue and
 *        are dispatched here. Every internal subsystem runs on this thread
 *        of control; no internal locking is required.
 */

#include <string.h>

#include "esp_log.h"
#include "esp_system.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_networking_api.h"

#include "blemesh_internal.h"

#define TAG "blemesh_task"

void blemesh_task_main(void *arg)
{
    (void)arg;
    blemesh_msg_t msg;
    for (;;) {
        if (xQueueReceive(g_blemesh_ctx.queue, &msg, portMAX_DELAY) == pdTRUE) {
            blemesh_handle_message(&msg);
        }
    }
}

void blemesh_handle_message(const blemesh_msg_t *msg)
{
    switch (msg->kind) {

    case BLEMESH_MSG_COMMISSION_ENABLE:
        (void)blemesh_provisioner_set_enabled(msg->u.commission.enable);
        break;

    case BLEMESH_MSG_COMMISSION_TIMEOUT:
        ESP_LOGI(TAG, "commissioning window expired");
        (void)blemesh_provisioner_set_enabled(false);
        break;

    case BLEMESH_MSG_REMOVE_NODE: {
        blemesh_addr_t addr = msg->u.remove_node.addr;
        blemesh_pending_clear_all_for(addr);
        blemesh_initial_sync_clear_for(addr);
        /* Tell the node itself to factory-reset (Config Node Reset).
         * Fire-and-forget — we delete the local entry regardless of
         * whether the node ACKs, so a missing node still gets purged. */
        if (g_blemesh_ctx.models.config_client) {
            esp_ble_mesh_client_common_param_t common = {
                .opcode      = ESP_BLE_MESH_MODEL_OP_NODE_RESET,
                .model       = g_blemesh_ctx.models.config_client,
                .ctx.net_idx = 0x0000,
                .ctx.app_idx = 0x0000,
                .ctx.addr    = addr,
                .ctx.send_ttl = g_blemesh_ctx.cfg.default_ttl,
                .msg_timeout = g_blemesh_ctx.cfg.set_confirm_timeout_ms,
            };
            esp_ble_mesh_cfg_client_set_state_t reset_state = { 0 };
            (void)esp_ble_mesh_config_client_set_state(&common, &reset_state);
        }
        /* Delete from provisioner's local node table so we forget keys etc. */
        (void)esp_ble_mesh_provisioner_delete_node_with_addr(addr);
        if (blemesh_dir_remove(addr) == ESP_OK && g_blemesh_ctx.cb.on_node_removed) {
            g_blemesh_ctx.cb.on_node_removed(addr);
        }
        break;
    }

    case BLEMESH_MSG_SET_STATE: {
        blemesh_addr_t addr = msg->u.set_state.addr;
        const blemesh_state_value_t *val = &msg->u.set_state.val;
        esp_err_t err = blemesh_dispatcher_send_set(addr, val,
                        msg->u.set_state.transition_ms);
        if (err == ESP_OK) {
            blemesh_pending_upsert(addr, val->id, val,
                                   g_blemesh_ctx.cfg.set_confirm_timeout_ms,
                                   g_blemesh_ctx.cfg.set_retries);
        } else if (g_blemesh_ctx.cb.on_set_failed) {
            g_blemesh_ctx.cb.on_set_failed(addr, val->id);
        }
        break;
    }

    case BLEMESH_MSG_GET_STATE:
        (void)blemesh_dispatcher_send_get(msg->u.get_state.addr,
                                          msg->u.get_state.state);
        break;

    case BLEMESH_MSG_REQUEST_SYNC:
        blemesh_initial_sync_enqueue(msg->u.request_sync.addr,
                                     msg->u.request_sync.profile);
        break;

    case BLEMESH_MSG_START_SYNC:
        /* Replay restored nodes to the consumer so it can rebuild any of its
         * own cached state, then schedule paced initial-state Gets so the
         * consumer's view converges to whatever the leaves are actually doing
         * right now (rather than the stale defaults the consumer's own NVS may
         * have persisted). Runs on the manager task so the sync FIFO is only
         * ever touched from this thread. */
        for (size_t i = 0; i < CONFIG_BLEMESH_MGR_MAX_NODES; i++) {
            if (!g_blemesh_ctx.nodes[i].used) {
                continue;
            }
            const blemesh_node_info_t *info = &g_blemesh_ctx.nodes[i].info;
            if (g_blemesh_ctx.cb.on_node_provisioned) {
                g_blemesh_ctx.cb.on_node_provisioned(info, false);
            }
            /* Enqueue an initial-state Get for every element so each element's
             * state converges to the leaf's actual state on reboot. */
            for (uint8_t ei = 0; ei < info->element_count; ei++) {
                blemesh_initial_sync_enqueue(
                    (blemesh_addr_t)(info->addr + ei), info->profile);
            }
        }
        break;

    case BLEMESH_MSG_VENDOR_SEND:
        (void)blemesh_dispatcher_send_vendor(msg->u.vendor.addr,
                                             msg->u.vendor.company_id,
                                             msg->u.vendor.opcode,
                                             msg->u.vendor.data,
                                             msg->u.vendor.len);
        break;

    case BLEMESH_MSG_PROV_EVT:
        blemesh_provisioner_on_event(msg);
        break;

    case BLEMESH_MSG_CFG_EVT:
        blemesh_configurator_on_reply(msg);
        /* cfg.data (COMPOSITION_DATA_STATUS payload allocated in
         * blemesh_provisioner.c's config_client_cb) is released by the
         * blemesh_free_msg tail call below, which handles cleanup for every
         * message kind that owns heap. */
        break;

    case BLEMESH_MSG_RX_STATUS:
        /* The dispatcher decodes the opcode into the correct
         * blemesh_state_id_t and clears the matching pending-confirmation
         * entry inside emit(). A blanket clear here used to default to
         * state id 0 (== BLEMESH_STATE_ONOFF) and wiped unrelated
         * outstanding OnOff confirmations whenever a Lightness / CTL /
         * HSL / Sensor status arrived from the same node. */
        blemesh_publication_rx_dispatch(msg);
        break;

    case BLEMESH_MSG_RX_HEARTBEAT: {
        blemesh_addr_t hb_addr = msg->u.heartbeat.addr;
        blemesh_heartbeat_on_packet(hb_addr);
        /* Treat every received heartbeat as a "the leaf is alive, may
         * have rebooted recently, re-pull state" signal. The unreachable
         * to reachable transition in heartbeat_on_packet only catches
         * leaves that fell below the loss threshold; a leaf that reboots
         * and rejoins within a single heartbeat period stays cached as
         * reachable and would otherwise drift silently from the manager.
         * Dedup in the FIFO keeps this from compounding when the FIFO
         * has not yet drained the previous round. The Status replies
         * arrive through the regular RX path and update the consumer's
         * cache via on_state_changed. */
        blemesh_node_entry_t *hb_node = blemesh_dir_find(hb_addr);
        if (hb_node != NULL && hb_node->used) {
            blemesh_initial_sync_enqueue(hb_addr, hb_node->info.profile);
        }
        break;
    }

    case BLEMESH_MSG_PENDING_TICK:
        blemesh_pending_on_tick();
        break;

    case BLEMESH_MSG_HEARTBEAT_TICK:
        blemesh_heartbeat_on_tick();
        break;

    case BLEMESH_MSG_SYNC_TICK:
        blemesh_initial_sync_on_tick();
        break;

    case BLEMESH_MSG_STOP:
        (void)blemesh_provisioner_set_enabled(false);
        break;

    case BLEMESH_MSG_NONE:
    default:
        ESP_LOGW(TAG, "unhandled msg kind %d", (int)msg->kind);
        break;
    }

    blemesh_free_msg(msg);
}
