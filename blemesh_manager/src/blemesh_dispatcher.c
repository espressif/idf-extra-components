/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file blemesh_dispatcher.c
 * @brief Outbound side: translate (state_id, value) into the right Set/Get
 *        opcode for the right Client model and hand it to the ESP-BLE-MESH
 *        stack. All Sets go out unacknowledged; confirmation is tracked via
 *        the pending-confirmation scheduler.
 */

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_lighting_model_api.h"
#include "esp_ble_mesh_sensor_model_api.h"
#include "esp_ble_mesh_networking_api.h"

#include "blemesh_internal.h"

#define TAG "blemesh_disp"

#define APP_KEY_IDX  0x0000
#define NET_KEY_IDX  0x0000

/* The consumer registers its Client model pointers via
 * blemesh_manager_register_models(); we read them out of the context. */

static esp_err_t common_param(esp_ble_mesh_client_common_param_t *out,
                              esp_ble_mesh_model_t *model,
                              uint32_t opcode,
                              blemesh_addr_t addr)
{
    if (model == NULL) {
        ESP_LOGE(TAG, "client model not registered for opcode 0x%08" PRIx32, (uint32_t)opcode);
        return ESP_ERR_INVALID_STATE;
    }
    memset(out, 0, sizeof(*out));
    out->opcode         = opcode;
    out->model          = model;
    out->ctx.net_idx    = NET_KEY_IDX;
    out->ctx.app_idx    = APP_KEY_IDX;
    out->ctx.addr       = addr;
    out->ctx.send_ttl   = g_blemesh_ctx.cfg.default_ttl;
    out->msg_timeout    = g_blemesh_ctx.cfg.set_confirm_timeout_ms;
    return ESP_OK;
}

esp_err_t blemesh_dispatcher_init(void)
{
    return ESP_OK;
}

/* Resolve the unicast address of a composite-light channel element (Light CTL
 * Temperature, Light HSL Hue/Saturation) given the node's primary/logical-element
 * address. `offset` is the parsed element offset (0 = unknown); `fallback` is
 * the SIG-conventional offset tried when Composition Data was unavailable but
 * the node clearly has enough elements. Returns 0 (caller should bail) when no
 * such element exists. */
static blemesh_addr_t channel_addr(blemesh_addr_t addr, uint8_t offset, uint8_t fallback)
{
    if (offset == 0) {
        blemesh_node_entry_t *e = blemesh_dir_find_by_element(addr);
        if (e && e->info.element_count > fallback) {
            offset = fallback;
        }
    }
    return (offset == 0) ? 0 : (blemesh_addr_t)(addr + offset);
}

/* ---------- Set ---------------------------------------------------------- */

esp_err_t blemesh_dispatcher_send_set(blemesh_addr_t addr,
                                      const blemesh_state_value_t *val,
                                      uint32_t transition_ms)
{
    esp_ble_mesh_client_common_param_t common;
    esp_err_t err = ESP_FAIL;
    static uint8_t s_tid;
    uint8_t tid = ++s_tid;
    uint8_t trans_time = 0;
    if (transition_ms > 0) {
        /* Generic Default Transition Time: 6-bit steps + 2-bit resolution. */
        uint32_t steps;
        uint8_t  res;
        if (transition_ms < 6300) {
            steps = transition_ms / 100; res = 0; /* 100 ms */
        } else if (transition_ms < 63000) {
            steps = transition_ms / 1000; res = 1; /* 1 s */
        } else if (transition_ms < 630000) {
            steps = transition_ms / 10000; res = 2; /* 10 s */
        } else {
            steps = transition_ms / 600000; res = 3; /* 10 min */
        }
        if (steps > 0x3E) {
            steps = 0x3E;
        }
        trans_time = (uint8_t)((res << 6) | (steps & 0x3F));
    }

    switch (val->id) {
    case BLEMESH_STATE_ONOFF: {
        esp_err_t er = common_param(&common,
                                    g_blemesh_ctx.models.generic_onoff_client,
                                    ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK,
                                    addr);
        if (er != ESP_OK) {
            return er;
        }
        esp_ble_mesh_generic_client_set_state_t set = {
            .onoff_set = {
                .op_en       = transition_ms > 0,
                .onoff       = val->v.onoff ? 1 : 0,
                .tid         = tid,
                .trans_time  = trans_time,
                .delay       = 0,
            },
        };
        err = esp_ble_mesh_generic_client_set_state(&common, &set);
        break;
    }
    case BLEMESH_STATE_LEVEL: {
        esp_err_t er = common_param(&common,
                                    g_blemesh_ctx.models.generic_level_client,
                                    ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_SET_UNACK,
                                    addr);
        if (er != ESP_OK) {
            return er;
        }
        esp_ble_mesh_generic_client_set_state_t set = {
            .level_set = {
                .op_en      = transition_ms > 0,
                .level      = val->v.level,
                .tid        = tid,
                .trans_time = trans_time,
                .delay      = 0,
            },
        };
        err = esp_ble_mesh_generic_client_set_state(&common, &set);
        break;
    }
    case BLEMESH_STATE_LIGHTNESS: {
        esp_err_t er = common_param(&common,
                                    g_blemesh_ctx.models.light_lightness_client,
                                    ESP_BLE_MESH_MODEL_OP_LIGHT_LIGHTNESS_SET_UNACK,
                                    addr);
        if (er != ESP_OK) {
            return er;
        }
        esp_ble_mesh_light_client_set_state_t set = {
            .lightness_set = {
                .op_en      = transition_ms > 0,
                .lightness  = val->v.lightness,
                .tid        = tid,
                .trans_time = trans_time,
                .delay      = 0,
            },
        };
        err = esp_ble_mesh_light_client_set_state(&common, &set);
        break;
    }
    case BLEMESH_STATE_CTL: {
        /* A temperature-only change carries the UNCHANGED lightness sentinel.
         * The combined Light CTL Set always re-asserts a lightness value, so a
         * temp-only change must instead go to the Light CTL Temperature Server
         * on its own (secondary) element, leaving brightness untouched. */
        if (val->v.ctl.lightness == BLEMESH_LIGHTNESS_UNCHANGED) {
            uint8_t temp_off = 0;
            blemesh_node_entry_t *e = blemesh_dir_find_by_element(addr);
            if (e) {
                temp_off = e->info.ctl_temp_offset;
                /* Composition Data unavailable but the node clearly has a
                 * secondary element: fall back to the SIG-conventional layout
                 * (Temperature element immediately after the primary). */
                if (temp_off == 0 && e->info.element_count >= 2) {
                    temp_off = 1;
                }
            }
            if (temp_off == 0) {
                ESP_LOGW(TAG, "no CTL Temperature element for 0x%04x — dropping temp-only set", addr);
                return ESP_ERR_INVALID_STATE;
            }
            blemesh_addr_t temp_addr = (blemesh_addr_t)(addr + temp_off);
            esp_err_t er = common_param(&common,
                                        g_blemesh_ctx.models.light_ctl_client,
                                        ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET_UNACK,
                                        temp_addr);
            if (er != ESP_OK) {
                return er;
            }
            esp_ble_mesh_light_client_set_state_t set = {
                .ctl_temperature_set = {
                    .op_en           = transition_ms > 0,
                    .ctl_temperature = val->v.ctl.temperature,
                    .ctl_delta_uv    = 0,
                    .tid             = tid,
                    .trans_time      = trans_time,
                    .delay           = 0,
                },
            };
            err = esp_ble_mesh_light_client_set_state(&common, &set);
            break;
        }
        esp_err_t er = common_param(&common,
                                    g_blemesh_ctx.models.light_ctl_client,
                                    ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_SET_UNACK,
                                    addr);
        if (er != ESP_OK) {
            return er;
        }
        esp_ble_mesh_light_client_set_state_t set = {
            .ctl_set = {
                .op_en          = transition_ms > 0,
                .ctl_lightness  = val->v.ctl.lightness,
                .ctl_temperature = val->v.ctl.temperature,
                .ctl_delta_uv   = 0,
                .tid            = tid,
                .trans_time     = trans_time,
                .delay          = 0,
            },
        };
        err = esp_ble_mesh_light_client_set_state(&common, &set);
        break;
    }
    case BLEMESH_STATE_HSL: {
        esp_err_t er = common_param(&common,
                                    g_blemesh_ctx.models.light_hsl_client,
                                    ESP_BLE_MESH_MODEL_OP_LIGHT_HSL_SET_UNACK,
                                    addr);
        if (er != ESP_OK) {
            return er;
        }
        esp_ble_mesh_light_client_set_state_t set = {
            .hsl_set = {
                .op_en          = transition_ms > 0,
                .hsl_lightness  = val->v.hsl.l,
                .hsl_hue        = val->v.hsl.h,
                .hsl_saturation = val->v.hsl.s,
                .tid            = tid,
                .trans_time     = trans_time,
                .delay          = 0,
            },
        };
        err = esp_ble_mesh_light_client_set_state(&common, &set);
        break;
    }
    case BLEMESH_STATE_HSL_HUE: {
        /* Hue-only change -> Light HSL Hue Set to the Hue element, so lightness
         * and saturation are untouched (the combined HSL Set re-asserts all
         * three). */
        blemesh_node_entry_t *e = blemesh_dir_find_by_element(addr);
        blemesh_addr_t hue_addr = channel_addr(addr, e ? e->info.hsl_hue_offset : 0, 1);
        if (hue_addr == 0) {
            ESP_LOGW(TAG, "no HSL Hue element for 0x%04x — dropping hue-only set", addr);
            return ESP_ERR_INVALID_STATE;
        }
        esp_err_t er = common_param(&common, g_blemesh_ctx.models.light_hsl_client,
                                    ESP_BLE_MESH_MODEL_OP_LIGHT_HSL_HUE_SET_UNACK, hue_addr);
        if (er != ESP_OK) {
            return er;
        }
        esp_ble_mesh_light_client_set_state_t set = {
            .hsl_hue_set = {
                .op_en      = transition_ms > 0,
                .hue        = val->v.hsl.h,
                .tid        = tid,
                .trans_time = trans_time,
                .delay      = 0,
            },
        };
        err = esp_ble_mesh_light_client_set_state(&common, &set);
        break;
    }
    case BLEMESH_STATE_HSL_SAT: {
        /* Saturation-only change -> Light HSL Saturation Set to the Saturation
         * element. */
        blemesh_node_entry_t *e = blemesh_dir_find_by_element(addr);
        blemesh_addr_t sat_addr = channel_addr(addr, e ? e->info.hsl_sat_offset : 0, 2);
        if (sat_addr == 0) {
            ESP_LOGW(TAG, "no HSL Saturation element for 0x%04x — dropping sat-only set", addr);
            return ESP_ERR_INVALID_STATE;
        }
        esp_err_t er = common_param(&common, g_blemesh_ctx.models.light_hsl_client,
                                    ESP_BLE_MESH_MODEL_OP_LIGHT_HSL_SATURATION_SET_UNACK, sat_addr);
        if (er != ESP_OK) {
            return er;
        }
        esp_ble_mesh_light_client_set_state_t set = {
            .hsl_saturation_set = {
                .op_en      = transition_ms > 0,
                .saturation = val->v.hsl.s,
                .tid        = tid,
                .trans_time = trans_time,
                .delay      = 0,
            },
        };
        err = esp_ble_mesh_light_client_set_state(&common, &set);
        break;
    }
    case BLEMESH_STATE_SENSOR_VALUE:
        /* Sensors are read-only at the model level. */
        return ESP_ERR_NOT_SUPPORTED;

    default:
        return ESP_ERR_INVALID_ARG;
    }

    return (err == ESP_OK) ? ESP_OK : ESP_FAIL;
}

/* ---------- Get ---------------------------------------------------------- */

esp_err_t blemesh_dispatcher_send_get(blemesh_addr_t addr,
                                      blemesh_state_id_t state_id)
{
    esp_ble_mesh_client_common_param_t common;
    esp_err_t err = ESP_FAIL;

    switch (state_id) {
    case BLEMESH_STATE_ONOFF: {
        esp_err_t er = common_param(&common, g_blemesh_ctx.models.generic_onoff_client,
                                    ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET, addr);
        if (er != ESP_OK) {
            return er;
        }
        err = esp_ble_mesh_generic_client_get_state(&common, NULL);
        break;
    }
    case BLEMESH_STATE_LEVEL: {
        esp_err_t er = common_param(&common, g_blemesh_ctx.models.generic_level_client,
                                    ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_GET, addr);
        if (er != ESP_OK) {
            return er;
        }
        err = esp_ble_mesh_generic_client_get_state(&common, NULL);
        break;
    }
    case BLEMESH_STATE_LIGHTNESS: {
        esp_err_t er = common_param(&common, g_blemesh_ctx.models.light_lightness_client,
                                    ESP_BLE_MESH_MODEL_OP_LIGHT_LIGHTNESS_GET, addr);
        if (er != ESP_OK) {
            return er;
        }
        err = esp_ble_mesh_light_client_get_state(&common, NULL);
        break;
    }
    case BLEMESH_STATE_CTL: {
        esp_err_t er = common_param(&common, g_blemesh_ctx.models.light_ctl_client,
                                    ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_GET, addr);
        if (er != ESP_OK) {
            return er;
        }
        err = esp_ble_mesh_light_client_get_state(&common, NULL);
        break;
    }
    case BLEMESH_STATE_HSL: {
        esp_err_t er = common_param(&common, g_blemesh_ctx.models.light_hsl_client,
                                    ESP_BLE_MESH_MODEL_OP_LIGHT_HSL_GET, addr);
        if (er != ESP_OK) {
            return er;
        }
        err = esp_ble_mesh_light_client_get_state(&common, NULL);
        break;
    }
    case BLEMESH_STATE_HSL_HUE: {
        blemesh_node_entry_t *e = blemesh_dir_find_by_element(addr);
        blemesh_addr_t hue_addr = channel_addr(addr, e ? e->info.hsl_hue_offset : 0, 1);
        if (hue_addr == 0) {
            return ESP_ERR_INVALID_STATE;
        }
        esp_err_t er = common_param(&common, g_blemesh_ctx.models.light_hsl_client,
                                    ESP_BLE_MESH_MODEL_OP_LIGHT_HSL_HUE_GET, hue_addr);
        if (er != ESP_OK) {
            return er;
        }
        err = esp_ble_mesh_light_client_get_state(&common, NULL);
        break;
    }
    case BLEMESH_STATE_HSL_SAT: {
        blemesh_node_entry_t *e = blemesh_dir_find_by_element(addr);
        blemesh_addr_t sat_addr = channel_addr(addr, e ? e->info.hsl_sat_offset : 0, 2);
        if (sat_addr == 0) {
            return ESP_ERR_INVALID_STATE;
        }
        esp_err_t er = common_param(&common, g_blemesh_ctx.models.light_hsl_client,
                                    ESP_BLE_MESH_MODEL_OP_LIGHT_HSL_SATURATION_GET, sat_addr);
        if (er != ESP_OK) {
            return er;
        }
        err = esp_ble_mesh_light_client_get_state(&common, NULL);
        break;
    }
    case BLEMESH_STATE_SENSOR_VALUE: {
        esp_err_t er = common_param(&common, g_blemesh_ctx.models.sensor_client,
                                    ESP_BLE_MESH_MODEL_OP_SENSOR_GET, addr);
        if (er != ESP_OK) {
            return er;
        }
        err = esp_ble_mesh_sensor_client_get_state(&common, NULL);
        break;
    }
    default:
        return ESP_ERR_INVALID_ARG;
    }
    return (err == ESP_OK) ? ESP_OK : ESP_FAIL;
}

/* ---------- Vendor ------------------------------------------------------- */

esp_err_t blemesh_dispatcher_send_vendor(blemesh_addr_t addr,
        uint16_t company_id,
        uint32_t opcode,
        const uint8_t *data,
        size_t len)
{
    /* Check if the opcode is a vendor opcode and the company ID is correct. */
    if ((opcode & 0xFF800000) != 0xC00000 ||
            company_id != (uint16_t)(opcode & 0xFFFF)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_mesh_model_t *model = g_blemesh_ctx.models.vendor_client;
    if (model == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_ble_mesh_msg_ctx_t ctx = {
        .net_idx  = NET_KEY_IDX,
        .app_idx  = APP_KEY_IDX,
        .addr     = addr,
        .send_ttl = g_blemesh_ctx.cfg.default_ttl,
    };
    esp_err_t err = esp_ble_mesh_client_model_send_msg(model, &ctx, opcode,
                    (uint16_t)len,
                    (uint8_t *)data,
                    0, false, ROLE_PROVISIONER);
    return (err == ESP_OK) ? ESP_OK : ESP_FAIL;
}
