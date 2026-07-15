/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file blemesh_manager.c
 * @brief Public-API surface and lifecycle glue. Every function here marshals
 *        its arguments into a queue message and returns; the heavy lifting
 *        happens on the manager task.
 */

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"

#include "blemesh_internal.h"

#define TAG "blemesh_mgr"

blemesh_ctx_t g_blemesh_ctx;

/* ---------- Internal helpers ---------------------------------------------- */

uint64_t blemesh_now_ms(void)
{
    return (uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
}

esp_err_t blemesh_post(const blemesh_msg_t *msg)
{
    if (g_blemesh_ctx.queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xQueueSend(g_blemesh_ctx.queue, msg, 0) != pdTRUE) {
        ESP_LOGE(TAG, "queue full, dropping msg kind=%d", (int)msg->kind);
        return ESP_FAIL;
    }
    return ESP_OK;
}

void blemesh_free_msg(const blemesh_msg_t *msg)
{
    switch (msg->kind) {
    case BLEMESH_MSG_VENDOR_SEND:
        free(msg->u.vendor.data);
        break;
    case BLEMESH_MSG_RX_STATUS:
        free(msg->u.rx_status.data);
        break;
    case BLEMESH_MSG_CFG_EVT:
        free(msg->u.cfg.data);
        break;
    default:
        break;
    }
}

static void commission_timer_cb(TimerHandle_t t)
{
    (void)t;
    blemesh_msg_t m = { .kind = BLEMESH_MSG_COMMISSION_TIMEOUT };
    (void)blemesh_post(&m);
}

/* ---------- Lifecycle ---------------------------------------------------- */

static blemesh_device_profile_t classify_comp(const uint8_t *data, size_t len,
        uint8_t *element_count)
{
    *element_count = 0;
    if (len < 10) {
        return BLEMESH_DEV_UNKNOWN;
    }
    bool onoff = false, level = false, lightness = false, ctl = false, hsl = false;
    bool sensor = false;
    size_t off = 10;
    while (off + 4 <= len) {
        uint8_t nums = data[off + 2];
        uint8_t numv = data[off + 3];
        off += 4;
        for (uint8_t i = 0; i < nums && off + 2 <= len; i++, off += 2) {
            uint16_t id = (uint16_t)(data[off] | ((uint16_t)data[off + 1] << 8));
            switch (id) {
            case 0x1000: onoff = true; break;        /* Gen OnOff Srv */
            case 0x1002: level = true; break;        /* Gen Level Srv */
            case 0x1300: lightness = true; break;    /* Light Lightness Srv */
            case 0x1303: ctl = true; break;          /* Light CTL Srv */
            case 0x1307: hsl = true; break;          /* Light HSL Srv */
            case 0x1100: sensor = true; break;       /* Sensor Srv */
            default: break;
            }
        }
        off += (size_t)numv * 4u;
        (*element_count)++;
    }
    if (hsl) {
        return BLEMESH_DEV_COLOR_HSL;
    }
    if (ctl) {
        return BLEMESH_DEV_COLOR_TEMP;
    }
    if (lightness || level) {
        return BLEMESH_DEV_DIMMABLE;
    }
    if (onoff) {
        return BLEMESH_DEV_ONOFF;
    }
    if (sensor) {
        return BLEMESH_DEV_TEMPERATURE;
    }
    return BLEMESH_DEV_VENDOR;
}

static void restore_directory_from_stack(void)
{
    uint16_t count = esp_ble_mesh_provisioner_get_prov_node_count();
    if (count == 0) {
        return;
    }
    const esp_ble_mesh_node_t **table = esp_ble_mesh_provisioner_get_node_table_entry();
    if (table == NULL) {
        return;
    }
    for (uint16_t i = 0; i < count; i++) {
        const esp_ble_mesh_node_t *n = table[i];
        if (n == NULL) {
            continue;
        }
        blemesh_node_info_t info = {
            .addr          = n->unicast_addr,
            .element_count = n->element_num,
            .company_id    = 0xFFFF,
        };
        if (n->comp_data && n->comp_length) {
            uint8_t ec = 0;
            info.profile = classify_comp(n->comp_data, n->comp_length, &ec);
            if (ec > 0) {
                info.element_count = ec;
            }
            blemesh_comp_logical_elem_offsets(n->comp_data, n->comp_length, info.profile,
                                              info.logical_elem_offset, &info.logical_elem_count);
            info.ctl_temp_offset = blemesh_comp_ctl_temp_offset(n->comp_data, n->comp_length);
            blemesh_comp_hsl_offsets(n->comp_data, n->comp_length,
                                     &info.hsl_hue_offset, &info.hsl_sat_offset);
        } else {
            /* No Composition Data persisted — fall back to a single logical
             * element at the primary element rather than guessing per-element
             * layout. */
            info.logical_elem_offset[0] = 0;
            info.logical_elem_count     = 1;
        }
        static const char hex[] = "0123456789abcdef";
        char *p = info.uuid;
        for (int j = 0; j < 16; j++) {
            *p++ = hex[(n->dev_uuid[j] >> 4) & 0xF];
            *p++ = hex[n->dev_uuid[j] & 0xF];
            if (j == 3 || j == 5 || j == 7 || j == 9) {
                *p++ = '-';
            }
        }
        *p = '\0';
        blemesh_node_entry_t *e = blemesh_dir_insert(&info);
        if (e) {
            /* Restored from NVS != reachable. Start offline; the first
             * heartbeat flips it online via on_reachability(true). */
            e->info.reachable = false;
        }
        ESP_LOGI(TAG, "restored node 0x%04x from NVS", info.addr);
    }
}

esp_err_t blemesh_manager_init(const blemesh_manager_config_t *cfg)
{
    if (cfg == NULL || cfg->prov == NULL || cfg->comp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (g_blemesh_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&g_blemesh_ctx, 0, sizeof(g_blemesh_ctx));
    g_blemesh_ctx.cfg = *cfg;
    if (g_blemesh_ctx.cfg.heartbeat_period_ms == 0) {
        g_blemesh_ctx.cfg.heartbeat_period_ms = 30000;
    }
    if (g_blemesh_ctx.cfg.set_confirm_timeout_ms == 0) {
        g_blemesh_ctx.cfg.set_confirm_timeout_ms = 4000;
    }
    if (g_blemesh_ctx.cfg.heartbeat_loss_threshold == 0) {
        g_blemesh_ctx.cfg.heartbeat_loss_threshold = 3;
    }
    if (g_blemesh_ctx.cfg.default_ttl == 0) {
        g_blemesh_ctx.cfg.default_ttl = 7;
    }
    if (g_blemesh_ctx.cfg.set_retries == 0) {
        g_blemesh_ctx.cfg.set_retries = 1;
    }

    esp_err_t err;

    g_blemesh_ctx.queue = xQueueCreate(CONFIG_BLEMESH_MGR_QUEUE_LENGTH,
                                       sizeof(blemesh_msg_t));
    if (g_blemesh_ctx.queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    BaseType_t r;
    if (CONFIG_BLEMESH_MGR_TASK_CORE_PIN < 0) {
        r = xTaskCreate(blemesh_task_main, "blemesh_mgr",
                        CONFIG_BLEMESH_MGR_TASK_STACK_SIZE, NULL,
                        CONFIG_BLEMESH_MGR_TASK_PRIORITY, &g_blemesh_ctx.task);
    } else {
        r = xTaskCreatePinnedToCore(blemesh_task_main, "blemesh_mgr",
                                    CONFIG_BLEMESH_MGR_TASK_STACK_SIZE, NULL,
                                    CONFIG_BLEMESH_MGR_TASK_PRIORITY,
                                    &g_blemesh_ctx.task,
                                    CONFIG_BLEMESH_MGR_TASK_CORE_PIN);
    }
    if (r != pdPASS) {
        vQueueDelete(g_blemesh_ctx.queue);
        g_blemesh_ctx.queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    /* Commissioning auto-close timer (period set later when armed). */
    g_blemesh_ctx.commission_timer = xTimerCreate("ble_mgr_comm",
                                     pdMS_TO_TICKS(1000),
                                     pdFALSE, NULL,
                                     commission_timer_cb);
    if (g_blemesh_ctx.commission_timer == NULL) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

    blemesh_dir_clear();
    blemesh_pending_init();
    blemesh_heartbeat_init();
    blemesh_initial_sync_init();

    /* 1. Register stack callbacks BEFORE esp_ble_mesh_init. */
    err = blemesh_provisioner_init();
    if (err != ESP_OK) {
        goto fail;
    }
    err = blemesh_publication_rx_init();
    if (err != ESP_OK) {
        goto fail;
    }
    err = blemesh_dispatcher_init();
    if (err != ESP_OK) {
        goto fail;
    }

    /* 3. ESP-BLE-MESH stack with caller-supplied prov + composition.
     *    This brings up the BTC task that the API calls below depend on,
     *    so anything that posts to the BTC thread (e.g. recv_heartbeat)
     *    must come AFTER this point — not in blemesh_provisioner_init. */
    err = esp_ble_mesh_init(g_blemesh_ctx.cfg.prov, g_blemesh_ctx.cfg.comp);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_mesh_init failed: %d", err);
        goto fail;
    }

    /* Opt into heartbeat reception now that the BTC task is alive. */
    err = blemesh_provisioner_enable_heartbeat_rx();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "enable_heartbeat_rx failed: %d", err);
    }

    /* 4. Enable provisioner role permanently so the primary NetKey is
     *    materialized and the manager retains its mesh identity.
     *    DO NOT disable later — that would tear down the NetKey subnet and
     *    break unicast sends to provisioned nodes. The commissioning window
     *    is gated separately at the beacon-handling layer (see provisioner). */
    err = esp_ble_mesh_provisioner_prov_enable(
              (esp_ble_mesh_prov_bearer_t)
              (ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "provisioner_prov_enable failed: %d", err);
        goto fail;
    }

    err = esp_ble_mesh_provisioner_add_local_app_key(
              g_blemesh_ctx.cfg.app_key, 0x0000, 0x0000);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "add_local_app_key failed: %d", err);
        goto fail;
    }

    /* Rebuild the volatile node directory from anything the stack has
     * persisted in NVS (so nodes provisioned in a previous boot are still
     * known to the manager and to higher layers). */
    restore_directory_from_stack();

    g_blemesh_ctx.initialized = true;
    ESP_LOGI(TAG, "initialized");
    return ESP_OK;

fail:
    /* Tear down whatever was created before the failure so a later
     * blemesh_manager_init() retry starts from a clean slate. */
    if (g_blemesh_ctx.commission_timer) {
        xTimerDelete(g_blemesh_ctx.commission_timer, 0);
        g_blemesh_ctx.commission_timer = NULL;
    }
    if (g_blemesh_ctx.task) {
        vTaskDelete(g_blemesh_ctx.task);
        g_blemesh_ctx.task = NULL;
    }
    if (g_blemesh_ctx.queue) {
        vQueueDelete(g_blemesh_ctx.queue);
        g_blemesh_ctx.queue = NULL;
    }
    return err;
}

esp_err_t blemesh_manager_deinit(void)
{
    if (!g_blemesh_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    (void)blemesh_manager_stop();

    if (g_blemesh_ctx.commission_timer) {
        xTimerStop(g_blemesh_ctx.commission_timer, 0);
        xTimerDelete(g_blemesh_ctx.commission_timer, 0);
        g_blemesh_ctx.commission_timer = NULL;
    }
    if (g_blemesh_ctx.pending_timer) {
        xTimerStop(g_blemesh_ctx.pending_timer, 0);
        xTimerDelete(g_blemesh_ctx.pending_timer, 0);
        g_blemesh_ctx.pending_timer = NULL;
    }
    if (g_blemesh_ctx.heartbeat_timer) {
        xTimerStop(g_blemesh_ctx.heartbeat_timer, 0);
        xTimerDelete(g_blemesh_ctx.heartbeat_timer, 0);
        g_blemesh_ctx.heartbeat_timer = NULL;
    }
    if (g_blemesh_ctx.sync_timer) {
        xTimerStop(g_blemesh_ctx.sync_timer, 0);
        xTimerDelete(g_blemesh_ctx.sync_timer, 0);
        g_blemesh_ctx.sync_timer = NULL;
    }
    blemesh_initial_sync_clear_all();

    if (g_blemesh_ctx.task) {
        vTaskDelete(g_blemesh_ctx.task);
        g_blemesh_ctx.task = NULL;
    }
    if (g_blemesh_ctx.queue) {
        vQueueDelete(g_blemesh_ctx.queue);
        g_blemesh_ctx.queue = NULL;
    }

    memset(&g_blemesh_ctx, 0, sizeof(g_blemesh_ctx));
    return ESP_OK;
}

esp_err_t blemesh_manager_start(void)
{
    if (!g_blemesh_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (g_blemesh_ctx.started) {
        return ESP_OK;
    }
    g_blemesh_ctx.started = true;
    blemesh_heartbeat_start();

    /* Replay restored nodes + seed paced initial-state Gets on the manager
     * task, not here: the sync FIFO is task-owned (see blemesh_initial_sync.c)
     * so it must not be mutated from the caller thread. */
    blemesh_msg_t m = { .kind = BLEMESH_MSG_START_SYNC };
    return blemesh_post(&m);
}

esp_err_t blemesh_manager_stop(void)
{
    if (!g_blemesh_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!g_blemesh_ctx.started) {
        return ESP_OK;
    }
    blemesh_msg_t m = { .kind = BLEMESH_MSG_STOP };
    (void)blemesh_post(&m);
    g_blemesh_ctx.started = false;
    blemesh_heartbeat_stop();
    return ESP_OK;
}

esp_err_t blemesh_manager_reset(void)
{
    if (!g_blemesh_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Stop first if still running, so timers are cancelled and the manager task is
     * left idle on its (empty) queue before we tear the stack down. */
    if (g_blemesh_ctx.started) {
        (void)blemesh_manager_stop();
    }

    /* Run the wipe inline (synchronously) rather than posting to the manager task:
     * callers drive a composite reset and reboot right after this returns (see the
     * header contract), so the erase MUST be durable by the time we return. Posting
     * it would let the erase race — and lose to — the caller's reboot. Does NOT
     * reboot; the caller does. */
    ESP_LOGW(TAG, "factory reset — wiping mesh NVS (no reboot)");
    blemesh_dir_clear();
    blemesh_pending_init();
    blemesh_initial_sync_clear_all();
    (void)blemesh_provisioner_set_enabled(false);

    /* erase_flash=true purges NVS-stored net/app keys, IV index, sequence numbers,
     * node list and composition data (settings_core_erase -> nvs_erase_all + commit). */
    esp_ble_mesh_deinit_param_t dp = { .erase_flash = true };
    esp_err_t err = esp_ble_mesh_deinit(&dp);

    if (err != ESP_OK) {
        return err;
    }

    /* Also tear down the manager's resources so a later init starts cleanly. */
    return blemesh_manager_deinit();
}

/* ---------- Callback registration ---------------------------------------- */

esp_err_t blemesh_manager_register_callbacks(const blemesh_manager_callbacks_t *cb)
{
    if (cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    g_blemesh_ctx.cb = *cb;
    return ESP_OK;
}

static void bind_local_sig_model(esp_ble_mesh_model_t *m)
{
    if (m == NULL) {
        return;
    }
    /* For SIG models the union's first 16 bits hold the SIG model ID. */
    uint16_t model_id = m->model_id;
    esp_err_t err = esp_ble_mesh_provisioner_bind_app_key_to_local_model(
                        g_blemesh_ctx.cfg.prov_addr_start,
                        0x0000,
                        model_id,
                        ESP_BLE_MESH_CID_NVAL);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "bind SIG model 0x%04x failed: %d", model_id, err);
    }
}

static void bind_local_vendor_model(esp_ble_mesh_model_t *m)
{
    if (m == NULL) {
        return;
    }
    esp_err_t err = esp_ble_mesh_provisioner_bind_app_key_to_local_model(
                        g_blemesh_ctx.cfg.prov_addr_start,
                        0x0000,
                        m->vnd.model_id,
                        m->vnd.company_id);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "bind vendor model 0x%04x cid 0x%04x failed: %d",
                 m->vnd.model_id, m->vnd.company_id, err);
    }
}

esp_err_t blemesh_manager_register_models(const blemesh_manager_models_t *models)
{
    if (models == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    g_blemesh_ctx.models = *models;

    bind_local_sig_model(models->config_client);
    bind_local_sig_model(models->generic_onoff_client);
    bind_local_sig_model(models->generic_level_client);
    bind_local_sig_model(models->light_lightness_client);
    bind_local_sig_model(models->light_ctl_client);
    bind_local_sig_model(models->light_hsl_client);
    bind_local_sig_model(models->sensor_client);
    bind_local_vendor_model(models->vendor_client);

    /* Subscribe every uplink Client model to the uplink group so the
     * provisioner receives every node's status publications. */
    static const uint16_t uplink_cli_ids[] = {
        ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI,
        ESP_BLE_MESH_MODEL_ID_GEN_LEVEL_CLI,
        ESP_BLE_MESH_MODEL_ID_LIGHT_LIGHTNESS_CLI,
        ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_CLI,
        ESP_BLE_MESH_MODEL_ID_LIGHT_HSL_CLI,
        ESP_BLE_MESH_MODEL_ID_SENSOR_CLI,
    };
    for (size_t i = 0; i < sizeof(uplink_cli_ids) / sizeof(uplink_cli_ids[0]); i++) {
        (void)esp_ble_mesh_model_subscribe_group_addr(
            g_blemesh_ctx.cfg.prov_addr_start, ESP_BLE_MESH_CID_NVAL,
            uplink_cli_ids[i],
            g_blemesh_ctx.cfg.uplink_group_addr);
    }
    return ESP_OK;
}

/* ---------- Commissioning control ---------------------------------------- */

esp_err_t blemesh_manager_enable_commissioning(bool enable, uint32_t window_seconds)
{
    if (!g_blemesh_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    blemesh_msg_t m = {
        .kind = BLEMESH_MSG_COMMISSION_ENABLE,
        .u.commission = { .enable = enable, .window_seconds = window_seconds },
    };
    esp_err_t err = blemesh_post(&m);
    if (err != ESP_OK) {
        return err;
    }

    if (enable && window_seconds > 0 && g_blemesh_ctx.commission_timer) {
        TickType_t period = pdMS_TO_TICKS((TickType_t)window_seconds * 1000U);
        xTimerChangePeriod(g_blemesh_ctx.commission_timer, period, 0);
    } else if (!enable && g_blemesh_ctx.commission_timer) {
        xTimerStop(g_blemesh_ctx.commission_timer, 0);
    }
    return ESP_OK;
}

esp_err_t blemesh_manager_remove_node(blemesh_addr_t addr)
{
    if (!g_blemesh_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    blemesh_msg_t m = {
        .kind = BLEMESH_MSG_REMOVE_NODE,
        .u.remove_node = { .addr = addr },
    };
    return blemesh_post(&m);
}

esp_err_t blemesh_manager_list_nodes(blemesh_node_info_t *out, size_t *count)
{
    if (count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!g_blemesh_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    blemesh_dir_snapshot(out, count);
    return ESP_OK;
}

esp_err_t blemesh_manager_get_node_info(blemesh_addr_t addr, blemesh_node_info_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!g_blemesh_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    blemesh_node_entry_t *e = blemesh_dir_find(addr);
    if (e == NULL || !e->used) {
        return ESP_ERR_NOT_FOUND;
    }
    *out = e->info;
    return ESP_OK;
}

/* ---------- Operations --------------------------------------------------- */

esp_err_t blemesh_manager_set_state(blemesh_addr_t addr,
                                    const blemesh_state_value_t *val,
                                    uint32_t transition_ms)
{
    if (val == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!g_blemesh_ctx.initialized || !g_blemesh_ctx.started) {
        return ESP_ERR_INVALID_STATE;
    }
    blemesh_msg_t m = {
        .kind = BLEMESH_MSG_SET_STATE,
        .u.set_state = {
            .addr          = addr,
            .val           = *val,
            .transition_ms = transition_ms,
        },
    };
    return blemesh_post(&m);
}

esp_err_t blemesh_manager_get_state(blemesh_addr_t addr, blemesh_state_id_t state)
{
    if (!g_blemesh_ctx.initialized || !g_blemesh_ctx.started) {
        return ESP_ERR_INVALID_STATE;
    }
    blemesh_msg_t m = {
        .kind = BLEMESH_MSG_GET_STATE,
        .u.get_state = { .addr = addr, .state = state },
    };
    return blemesh_post(&m);
}

esp_err_t blemesh_manager_request_sync(blemesh_addr_t addr,
                                       blemesh_device_profile_t profile)
{
    if (!g_blemesh_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    /* Marshal onto the manager task — the sync FIFO is task-owned and must
     * not be touched from an arbitrary caller thread (would race the task /
     * timer tick). See blemesh_initial_sync.c. */
    blemesh_msg_t m = {
        .kind = BLEMESH_MSG_REQUEST_SYNC,
        .u.request_sync = { .addr = addr, .profile = profile },
    };
    return blemesh_post(&m);
}

esp_err_t blemesh_manager_send_vendor(blemesh_addr_t addr,
                                      uint16_t company_id,
                                      uint32_t opcode,
                                      const uint8_t *data,
                                      size_t len)
{
    if (!g_blemesh_ctx.initialized || !g_blemesh_ctx.started) {
        return ESP_ERR_INVALID_STATE;
    }
    if (len > 0 && data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t *copy = NULL;
    if (len > 0) {
        copy = malloc(len);
        if (copy == NULL) {
            return ESP_ERR_NO_MEM;
        }
        memcpy(copy, data, len);
    }
    blemesh_msg_t m = {
        .kind = BLEMESH_MSG_VENDOR_SEND,
        .u.vendor = {
            .addr       = addr,
            .company_id = company_id,
            .opcode     = opcode,
            .len        = len,
            .data       = copy,
        },
    };
    esp_err_t err = blemesh_post(&m);
    if (err != ESP_OK) {
        blemesh_free_msg(&m);
    }
    return err;
}
