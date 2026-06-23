/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file blemesh_publication_rx.c
 * @brief Single fan-in for every inbound Status message — generic, lighting,
 *        sensor, vendor. Decodes the opcode into a (state_id, value), fires
 *        on_state_changed, clears the matching pending-confirmation entry,
 *        and feeds the heartbeat tracker.
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_lighting_model_api.h"
#include "esp_ble_mesh_sensor_model_api.h"

#include "blemesh_internal.h"

#define TAG "blemesh_rx"

/* ---------- Stack-callback marshaling (named in provisioner) ------------- */

void blemesh_generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event,
                               esp_ble_mesh_generic_client_cb_param_t *param)
{
    if (param == NULL || param->params == NULL) {
        return;
    }
    blemesh_msg_t m = {
        .kind = BLEMESH_MSG_RX_STATUS,
        .u.rx_status = {
            .addr   = param->params->ctx.addr,
            .opcode = param->params->opcode,
        },
    };
    /* For status-bearing events, copy the value payload. We never need >16 B
     * for the v1 catalogue. */
    if (event == ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT ||
            event == ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT ||
            event == ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT) {
        esp_ble_mesh_gen_client_status_cb_t *buf = malloc(sizeof(*buf));
        if (buf) {
            *buf = param->status_cb;
            m.u.rx_status.data = (uint8_t *)buf;
            m.u.rx_status.len  = sizeof(*buf);
        }
    }
    if (blemesh_post(&m) != ESP_OK) {
        blemesh_free_msg(&m);
    }
}

void blemesh_lighting_client_cb(esp_ble_mesh_light_client_cb_event_t event,
                                esp_ble_mesh_light_client_cb_param_t *param)
{
    if (param == NULL || param->params == NULL) {
        return;
    }
    blemesh_msg_t m = {
        .kind = BLEMESH_MSG_RX_STATUS,
        .u.rx_status = {
            .addr   = param->params->ctx.addr,
            .opcode = param->params->opcode,
        },
    };
    if (event == ESP_BLE_MESH_LIGHT_CLIENT_PUBLISH_EVT ||
            event == ESP_BLE_MESH_LIGHT_CLIENT_GET_STATE_EVT ||
            event == ESP_BLE_MESH_LIGHT_CLIENT_SET_STATE_EVT) {
        esp_ble_mesh_light_client_status_cb_t *buf = malloc(sizeof(*buf));
        if (buf) {
            *buf = param->status_cb;
            m.u.rx_status.data = (uint8_t *)buf;
            m.u.rx_status.len  = sizeof(*buf);
        }
    }
    if (blemesh_post(&m) != ESP_OK) {
        blemesh_free_msg(&m);
    }
}

void blemesh_sensor_client_cb(esp_ble_mesh_sensor_client_cb_event_t event,
                              esp_ble_mesh_sensor_client_cb_param_t *param)
{
    if (param == NULL || param->params == NULL) {
        return;
    }
    blemesh_msg_t m = {
        .kind = BLEMESH_MSG_RX_STATUS,
        .u.rx_status = {
            .addr   = param->params->ctx.addr,
            .opcode = param->params->opcode,
        },
    };
    /* For Sensor Status events, the actual Marshalled Sensor Data bytes
     * live inside `status_cb.sensor_status.marshalled_sensor_data` (a
     * `struct net_buf_simple *`). The stack frees that buffer once we
     * return, so copy the raw bytes synchronously here. We never need
     * more than 64 B for the v1 sensor support (one or two MPID
     * entries; the task-side parser only surfaces the first). */
    if ((event == ESP_BLE_MESH_SENSOR_CLIENT_PUBLISH_EVT ||
            event == ESP_BLE_MESH_SENSOR_CLIENT_GET_STATE_EVT) &&
            param->params->opcode == ESP_BLE_MESH_MODEL_OP_SENSOR_STATUS &&
            param->status_cb.sensor_status.marshalled_sensor_data != NULL) {
        struct net_buf_simple *buf =
                param->status_cb.sensor_status.marshalled_sensor_data;
        size_t copy_len = buf->len;
        if (copy_len > 64) {
            copy_len = 64;
        }
        if (copy_len > 0) {
            m.u.rx_status.data = malloc(copy_len);
            if (m.u.rx_status.data) {
                memcpy(m.u.rx_status.data, buf->data, copy_len);
                m.u.rx_status.len = copy_len;
            }
        }
    }
    if (blemesh_post(&m) != ESP_OK) {
        blemesh_free_msg(&m);
    }
}

/* ---------- Public init -------------------------------------------------- */

esp_err_t blemesh_publication_rx_init(void)
{
    /* Client-model callbacks already registered in blemesh_provisioner_init. */
    return ESP_OK;
}

/* ---------- Task-side decode -------------------------------------------- */

static void emit(blemesh_addr_t addr, blemesh_state_id_t id,
                 const blemesh_state_value_t *val)
{
    if (g_blemesh_ctx.cb.on_state_changed) {
        g_blemesh_ctx.cb.on_state_changed(addr, val);
    }
    blemesh_pending_clear(addr, id);
}

void blemesh_publication_rx_dispatch(const blemesh_msg_t *msg)
{
    blemesh_addr_t addr = msg->u.rx_status.addr;
    uint32_t opcode     = msg->u.rx_status.opcode;
    blemesh_state_value_t val = { 0 };

    blemesh_node_entry_t *owner = blemesh_dir_find_by_element(addr);

    /* Any traffic from the node also resets its heartbeat counter — implicit
     * liveness signal. Use the node's primary address so publications from
     * secondary/channel/gang elements still mark the node reachable. */
    blemesh_heartbeat_on_packet(owner ? owner->info.addr : addr);

    /* Normalize the source address. A Status may originate from a channel
     * element that is not a logical element of its own — e.g. the Light CTL
     * Temperature element or the Light HSL Hue/Saturation elements. Those belong
     * to the node's primary logical element, so remap them to the node's primary
     * address. Element addresses that are themselves logical elements (each gang
     * of a multi-gang switch) are left untouched so they still route per-gang. */
    /* Classify the source element. Composite-light channel elements (HSL
     * Hue/Saturation, CTL Temperature) each carry a Generic Level Server whose
     * level IS that channel (hue/saturation/temperature), not brightness — and
     * the node may publish a channel change via the Generic Level Server, the
     * channel-specific server, or both. We must report a channel's Generic Level
     * as the matching channel state, never as a plain Level. */
    enum { CH_NONE, CH_HUE, CH_SAT, CH_TEMP, CH_OTHER } channel = CH_NONE;
    if (owner) {
        uint8_t offset          = (uint8_t)(addr - owner->info.addr);
        bool    is_logical_elem = false;
        for (uint8_t i = 0; i < owner->info.logical_elem_count; i++) {
            if (owner->info.logical_elem_offset[i] == offset) {
                is_logical_elem = true;
                break;
            }
        }
        if (!is_logical_elem) {
            if (offset != 0 && offset == owner->info.hsl_hue_offset) {
                channel = CH_HUE;
            } else if (offset != 0 && offset == owner->info.hsl_sat_offset) {
                channel = CH_SAT;
            } else if (offset != 0 && offset == owner->info.ctl_temp_offset) {
                channel = CH_TEMP;
            } else {
                channel = CH_OTHER;
            }
            addr = owner->info.addr;
        }
    }

    switch (opcode) {

    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS:
        if (msg->u.rx_status.data && msg->u.rx_status.len >= sizeof(esp_ble_mesh_gen_client_status_cb_t)) {
            const esp_ble_mesh_gen_client_status_cb_t *s =
                (const esp_ble_mesh_gen_client_status_cb_t *)msg->u.rx_status.data;
            val.id      = BLEMESH_STATE_ONOFF;
            val.v.onoff = s->onoff_status.present_onoff != 0;
            emit(addr, val.id, &val);
        }
        break;

    case ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_STATUS:
        if (msg->u.rx_status.data && msg->u.rx_status.len >= sizeof(esp_ble_mesh_gen_client_status_cb_t)) {
            const esp_ble_mesh_gen_client_status_cb_t *s =
                (const esp_ble_mesh_gen_client_status_cb_t *)msg->u.rx_status.data;
            int16_t lvl = (int16_t)s->level_status.present_level;
            /* The HSL Hue/Saturation state IS the Generic Level + 32768 (the
             * state binding). Route a channel element's Generic Level to its
             * channel; only the primary element's level is brightness. */
            switch (channel) {
            case CH_NONE:
                val.id      = BLEMESH_STATE_LEVEL;
                val.v.level = lvl;
                emit(addr, val.id, &val);
                break;
            case CH_HUE:
                val.id      = BLEMESH_STATE_HSL_HUE;
                val.v.hsl.h = (uint16_t)((int32_t)lvl + 32768);
                emit(addr, val.id, &val);
                break;
            case CH_SAT:
                val.id      = BLEMESH_STATE_HSL_SAT;
                val.v.hsl.s = (uint16_t)((int32_t)lvl + 32768);
                emit(addr, val.id, &val);
                break;
            case CH_TEMP:   /* nonlinear level<->Kelvin; rely on CTL Temperature Status */
            case CH_OTHER:  /* unknown channel — don't mistake it for brightness */
            default:
                break;
            }
        }
        break;

    case ESP_BLE_MESH_MODEL_OP_LIGHT_LIGHTNESS_STATUS:
        if (msg->u.rx_status.data && msg->u.rx_status.len >= sizeof(esp_ble_mesh_light_client_status_cb_t)) {
            const esp_ble_mesh_light_client_status_cb_t *s =
                (const esp_ble_mesh_light_client_status_cb_t *)msg->u.rx_status.data;
            val.id          = BLEMESH_STATE_LIGHTNESS;
            val.v.lightness = s->lightness_status.present_lightness;
            /* Reserve 0xFFFF as the UNCHANGED sentinel: a leaf reporting true
             * max lightness must not be mistaken for "no lightness". */
            if (val.v.lightness == BLEMESH_LIGHTNESS_UNCHANGED) {
                val.v.lightness = 0xFFFE;
            }
            emit(addr, val.id, &val);
        }
        break;

    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_STATUS:
        if (msg->u.rx_status.data && msg->u.rx_status.len >= sizeof(esp_ble_mesh_light_client_status_cb_t)) {
            const esp_ble_mesh_light_client_status_cb_t *s =
                (const esp_ble_mesh_light_client_status_cb_t *)msg->u.rx_status.data;
            val.id                = BLEMESH_STATE_CTL;
            val.v.ctl.lightness   = s->ctl_status.present_ctl_lightness;
            val.v.ctl.temperature = s->ctl_status.present_ctl_temperature;
            /* Reserve 0xFFFF as the UNCHANGED sentinel (see Lightness Status). */
            if (val.v.ctl.lightness == BLEMESH_LIGHTNESS_UNCHANGED) {
                val.v.ctl.lightness = 0xFFFE;
            }
            emit(addr, val.id, &val);
        }
        break;

    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS:
        /* Color-temperature changes made on the device flow through the Light
         * CTL Temperature Server, which lives on a secondary element and emits
         * this opcode (not the full CTL Status). It carries temperature only,
         * so flag lightness as unchanged and let the consumer update just the
         * color-temperature value. */
        if (msg->u.rx_status.data && msg->u.rx_status.len >= sizeof(esp_ble_mesh_light_client_status_cb_t)) {
            const esp_ble_mesh_light_client_status_cb_t *s =
                (const esp_ble_mesh_light_client_status_cb_t *)msg->u.rx_status.data;
            val.id                = BLEMESH_STATE_CTL;
            val.v.ctl.lightness   = BLEMESH_LIGHTNESS_UNCHANGED;
            val.v.ctl.temperature = s->ctl_temperature_status.present_ctl_temperature;
            emit(addr, val.id, &val);
        }
        break;

    case ESP_BLE_MESH_MODEL_OP_LIGHT_HSL_STATUS:
        if (msg->u.rx_status.data && msg->u.rx_status.len >= sizeof(esp_ble_mesh_light_client_status_cb_t)) {
            const esp_ble_mesh_light_client_status_cb_t *s =
                (const esp_ble_mesh_light_client_status_cb_t *)msg->u.rx_status.data;
            val.id        = BLEMESH_STATE_HSL;
            val.v.hsl.l   = s->hsl_status.hsl_lightness;
            val.v.hsl.h   = s->hsl_status.hsl_hue;
            val.v.hsl.s   = s->hsl_status.hsl_saturation;
            emit(addr, val.id, &val);
        }
        break;

    case ESP_BLE_MESH_MODEL_OP_LIGHT_HSL_HUE_STATUS:
        /* Hue changed on the device via the Light HSL Hue Server (a secondary
         * element). Carries hue only. */
        if (msg->u.rx_status.data && msg->u.rx_status.len >= sizeof(esp_ble_mesh_light_client_status_cb_t)) {
            const esp_ble_mesh_light_client_status_cb_t *s =
                (const esp_ble_mesh_light_client_status_cb_t *)msg->u.rx_status.data;
            val.id      = BLEMESH_STATE_HSL_HUE;
            val.v.hsl.h = s->hsl_hue_status.present_hue;
            emit(addr, val.id, &val);
        }
        break;

    case ESP_BLE_MESH_MODEL_OP_LIGHT_HSL_SATURATION_STATUS:
        if (msg->u.rx_status.data && msg->u.rx_status.len >= sizeof(esp_ble_mesh_light_client_status_cb_t)) {
            const esp_ble_mesh_light_client_status_cb_t *s =
                (const esp_ble_mesh_light_client_status_cb_t *)msg->u.rx_status.data;
            val.id      = BLEMESH_STATE_HSL_SAT;
            val.v.hsl.s = s->hsl_saturation_status.present_saturation;
            emit(addr, val.id, &val);
        }
        break;

    case ESP_BLE_MESH_MODEL_OP_SENSOR_STATUS:
        /* Sensor Status payload is one or more (MPID, Value) tuples.
         *
         * Marshalled Property ID (MPID) header formats per Mesh spec 4.2.14:
         *   Format A (2 octets, bit0 == 0):
         *     [0]: bit0=0, bits1..4 = length-1, bits5..7 = PID bits 0..2
         *     [1]: PID bits 3..10                    (11-bit Property ID)
         *   Format B (3 octets, bit0 == 1):
         *     [0]: bit0=1, bits1..7 = length-1 (0x7F => zero data)
         *     [1..2]: Property ID                    (16-bit, little-endian)
         *
         * We surface only the first MPID; multi-property Sensor Status is
         * documented as a known follow-up. */
        if (msg->u.rx_status.data && msg->u.rx_status.len >= 2) {
            const uint8_t *p   = (const uint8_t *)msg->u.rx_status.data;
            size_t         len = msg->u.rx_status.len;
            uint16_t       property_id = 0;
            size_t         hdr_len     = 0;
            size_t         data_len    = 0;
            bool           ok          = false;

            if ((p[0] & 0x01) == 0) {
                /* Format A */
                hdr_len     = 2;
                data_len    = ((size_t)((p[0] >> 1) & 0x0F)) + 1u;
                property_id = (uint16_t)(((p[0] >> 5) & 0x07) |
                                         ((uint16_t)p[1] << 3));
                ok = (len >= hdr_len + data_len);
            } else if (len >= 3) {
                /* Format B */
                hdr_len = 3;
                uint8_t len_field = (uint8_t)((p[0] >> 1) & 0x7F);
                data_len = (len_field == 0x7F) ? 0 : (size_t)len_field + 1u;
                property_id = (uint16_t)(p[1] | ((uint16_t)p[2] << 8));
                ok = (len >= hdr_len + data_len);
            }

            if (ok) {
                val.id                   = BLEMESH_STATE_SENSOR_VALUE;
                val.v.sensor.property_id = property_id;
                size_t cp = data_len < sizeof(val.v.sensor.data)
                            ? data_len
                            : sizeof(val.v.sensor.data);
                if (cp > 0) {
                    memcpy(val.v.sensor.data, p + hdr_len, cp);
                }
                val.v.sensor.len = (uint8_t)cp;
                emit(addr, val.id, &val);
            } else {
                ESP_LOGW(TAG, "malformed Sensor Status from 0x%04x len=%u",
                         addr, (unsigned)len);
            }
        }
        break;

    default:
        /* Only genuine vendor traffic goes to the consumer. A 3-octet vendor
         * opcode carries the ESP_BLE_MESH_MODEL_OP_3 marker (0xC00000) and its
         * company id in the low 16 bits, so it is always > 0xFFFF. Anything
         * <= 0xFFFF is an unhandled SIG opcode — leaves periodically emit SIG
         * Get messages (OnOff Get 0x8201, Lightness Get 0x824B, CTL Get
         * 0x825D, HSL Get 0x826D) onto the uplink group; we consume Status,
         * not Get, so those are expected and merely debug-logged rather than
         * mislabelled as vendor messages. */
        if (opcode > 0xFFFF) {
            if (g_blemesh_ctx.cb.on_vendor_message) {
                g_blemesh_ctx.cb.on_vendor_message(addr,
                                                   (uint16_t)(opcode & 0xFFFF), /* company id */
                                                   opcode,
                                                   msg->u.rx_status.data,
                                                   msg->u.rx_status.len);
            } else {
                ESP_LOGD(TAG, "unhandled vendor opcode 0x%08" PRIx32 " from 0x%04" PRIx16,
                         (uint32_t)opcode, (uint16_t)addr);
            }
        } else {
            ESP_LOGD(TAG, "unhandled SIG opcode 0x%08" PRIx32 " from 0x%04" PRIx16,
                     (uint32_t)opcode, (uint16_t)addr);
        }
        break;
    }
}
