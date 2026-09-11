/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file blemesh_provisioner.c
 * @brief ESP-BLE-MESH stack init, provisioner registration, unprovisioned
 *        beacon scanning, PB-ADV / PB-GATT link orchestration. Stack
 *        callbacks marshal their arguments into the manager queue; no
 *        consumer-facing work happens in stack context.
 */

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_err.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_lighting_model_api.h"
#include "esp_ble_mesh_sensor_model_api.h"

#include "blemesh_internal.h"

#define TAG "blemesh_prov"

/* Provisioner's own primary element / unicast address — first address in the
 * provisioner's allocated range. */
#define PROVISIONER_ELEM_COUNT  1

/* ---------- Stack-callback marshaling ------------------------------------ */

static void provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                            esp_ble_mesh_prov_cb_param_t *param)
{
    /* Beacon arrivals: handled in stack context. add_unprov_dev dedups by
     * UUID internally, so calling on every beacon is cheap and idempotent;
     * we do NOT post to the manager queue (would flood it). */
    if (event == ESP_BLE_MESH_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT) {
        if (!g_blemesh_ctx.commissioning_open) {
            return;
        }
        /* Serialize: only one unprovisioned device is taken at a time.
         * Cleared when configurator finishes or aborts the current node. */
        if (g_blemesh_ctx.provisioning_busy) {
            return;
        }
        esp_ble_mesh_unprov_dev_add_t add = { 0 };
        memcpy(add.addr, param->provisioner_recv_unprov_adv_pkt.addr,
               sizeof(add.addr));
        add.addr_type = param->provisioner_recv_unprov_adv_pkt.addr_type;
        memcpy(add.uuid, param->provisioner_recv_unprov_adv_pkt.dev_uuid, 16);
        add.oob_info  = param->provisioner_recv_unprov_adv_pkt.oob_info;
        /* The unprov device may keep advertising on whichever bearer it
         * supports — typically both PB-ADV and PB-GATT, alternated by the
         * IDF mesh stack. The stack rejects (and BT_WARNs) a beacon whose
         * bearer doesn't intersect the queued bitmask, so OR both bearers
         * here: if the leaf only ever advertises on one of them we still
         * pass through; if it advertises on both we don't spam the log
         * with "Device in queue not support PB-..." every poll. */
        add.bearer    = (esp_ble_mesh_prov_bearer_t)
                        (ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
        if (esp_ble_mesh_provisioner_add_unprov_dev(
                    &add,
                    ADD_DEV_RM_AFTER_PROV_FLAG | ADD_DEV_FLUSHABLE_DEV_FLAG) == ESP_OK) {
            g_blemesh_ctx.provisioning_busy = true;
        }
        return;
    }

    /* Heartbeat reception: post a tracker reset for the source. */
    if (event == ESP_BLE_MESH_PROVISIONER_RECV_HEARTBEAT_MESSAGE_EVT) {
        blemesh_msg_t hb = {
            .kind = BLEMESH_MSG_RX_HEARTBEAT,
            .u.heartbeat = {
                .addr = param->provisioner_recv_heartbeat.hb_src,
                .rssi = (uint8_t)param->provisioner_recv_heartbeat.rssi,
            },
        };
        (void)blemesh_post(&hb);
        return;
    }

    blemesh_msg_t m = { .kind = BLEMESH_MSG_PROV_EVT };
    m.u.prov.event = (int)event;
    if (event == ESP_BLE_MESH_PROVISIONER_PROV_COMPLETE_EVT) {
        m.u.prov.addr = param->provisioner_prov_complete.unicast_addr;
        memcpy(m.u.prov.uuid,
               param->provisioner_prov_complete.device_uuid, 16);
    }
    (void)blemesh_post(&m);
}

static void config_client_cb(esp_ble_mesh_cfg_client_cb_event_t event,
                             esp_ble_mesh_cfg_client_cb_param_t *param)
{
    (void)event;
    if (param == NULL || param->params == NULL) {
        return;
    }
    blemesh_msg_t m = { .kind = BLEMESH_MSG_CFG_EVT };
    m.u.cfg.addr   = param->params->ctx.addr;
    m.u.cfg.opcode = param->params->opcode;
    /* TIMEOUT_EVT does not populate error_code — flag it explicitly so the
     * configurator aborts instead of treating it as a success. */
    if (event == ESP_BLE_MESH_CFG_CLIENT_TIMEOUT_EVT) {
        m.u.cfg.status = -1;
    } else {
        m.u.cfg.status = (int)param->error_code;
        /* Echo element+model so the configurator can reject a stale/duplicate
         * STATUS that belongs to an already-advanced BIND/PUB cursor. The
         * status_cb union is only valid on real status events, not TIMEOUT. */
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND) {
            m.u.cfg.elem_addr = param->status_cb.model_app_status.element_addr;
            m.u.cfg.model_id  = param->status_cb.model_app_status.model_id;
        } else if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET) {
            m.u.cfg.elem_addr = param->status_cb.model_pub_status.element_addr;
            m.u.cfg.model_id  = param->status_cb.model_pub_status.model_id;
        }
    }

    /* Composition Data Status carries the payload we need to classify the
     * node. Copy it so the manager task can parse on its own thread. */
    if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET) {
        struct net_buf_simple *buf = param->status_cb.comp_data_status.composition_data;
        if (buf && buf->len > 0) {
            m.u.cfg.data = malloc(buf->len);
            if (m.u.cfg.data) {
                memcpy(m.u.cfg.data, buf->data, buf->len);
                m.u.cfg.len = buf->len;
            }
        }
    }
    if (blemesh_post(&m) != ESP_OK) {
        blemesh_free_msg(&m);
    }
}

/* Forward declarations for client model callbacks defined in publication_rx.c
 * — they share the same queue dispatch pattern. */
extern void blemesh_generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event,
                                      esp_ble_mesh_generic_client_cb_param_t *param);
extern void blemesh_lighting_client_cb(esp_ble_mesh_light_client_cb_event_t event,
                                       esp_ble_mesh_light_client_cb_param_t *param);
extern void blemesh_sensor_client_cb(esp_ble_mesh_sensor_client_cb_event_t event,
                                     esp_ble_mesh_sensor_client_cb_param_t *param);

/* ---------- Public init -------------------------------------------------- */

esp_err_t blemesh_provisioner_init(void)
{
    esp_err_t err = esp_ble_mesh_register_prov_callback(provisioning_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register_prov_callback failed: %d", err);
        return ESP_FAIL;
    }
    err = esp_ble_mesh_register_config_client_callback(config_client_cb);
    if (err != ESP_OK) {
        return ESP_FAIL;
    }
    err = esp_ble_mesh_register_generic_client_callback(blemesh_generic_client_cb);
    if (err != ESP_OK) {
        return ESP_FAIL;
    }
    err = esp_ble_mesh_register_light_client_callback(blemesh_lighting_client_cb);
    if (err != ESP_OK) {
        return ESP_FAIL;
    }
    err = esp_ble_mesh_register_sensor_client_callback(blemesh_sensor_client_cb);
    if (err != ESP_OK) {
        return ESP_FAIL;
    }
    /* NOTE: `esp_ble_mesh_provisioner_recv_heartbeat(true)` posts to the
     * BTC task and so cannot run before `esp_ble_mesh_init`. The manager
     * enables heartbeat reception in `blemesh_manager_init` immediately
     * after `esp_ble_mesh_init` succeeds. */
    return ESP_OK;
}

esp_err_t blemesh_provisioner_enable_heartbeat_rx(void)
{
    /* Provisioner does not receive heartbeat messages by default; opt in.
     * Must be invoked AFTER esp_ble_mesh_init has brought up the BTC task. */
    return esp_ble_mesh_provisioner_recv_heartbeat(true);
}

esp_err_t blemesh_provisioner_set_enabled(bool enable)
{
    /* The stack-level provisioner role is enabled permanently at init —
     * tearing it down via prov_disable() also tears down the primary NetKey
     * subnet, which breaks unicast messaging to all already-provisioned
     * nodes. The commissioning window is therefore gated at the beacon-rx
     * layer with a single flag. */
    g_blemesh_ctx.commissioning_open = enable;
    ESP_LOGI(TAG, "commissioning window %s", enable ? "OPEN" : "CLOSED");
    return ESP_OK;
}

/* ---------- Task-side event handling ------------------------------------- */

void blemesh_provisioner_on_event(const blemesh_msg_t *msg)
{
    int event = msg->u.prov.event;

    switch (event) {
    case ESP_BLE_MESH_PROVISIONER_PROV_COMPLETE_EVT:
        ESP_LOGI(TAG, "provisioning complete addr=0x%04x", msg->u.prov.addr);
        blemesh_configurator_start(msg->u.prov.addr, msg->u.prov.uuid);
        break;

    case ESP_BLE_MESH_PROVISIONER_PROV_LINK_OPEN_EVT:
        /* Informational. */
        break;

    case ESP_BLE_MESH_PROVISIONER_PROV_LINK_CLOSE_EVT:
        /* If provisioning failed (link closed without PROV_COMPLETE having
         * armed the configurator), release the serialization slot so the
         * next beacon can be accepted. When provisioning succeeded the
         * configurator owns the slot and clears it on completion. */
        if (!blemesh_configurator_busy()) {
            g_blemesh_ctx.provisioning_busy = false;
        }
        break;

    default:
        ESP_LOGD(TAG, "prov event %d", event);
        break;
    }
}
