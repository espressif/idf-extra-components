/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file blemesh_configurator.c
 * @brief Post-provisioning Config Client sequence:
 *          Composition Data Get → AppKey Add → Model App Bind (per server) →
 *          Model Publication Set → Heartbeat Publication Set → Default TTL Set
 *        On success the node is inserted into the directory and the
 *        on_node_provisioned callback fires.
 *
 * The state machine runs strictly on the manager task. Each outbound Config
 * Client call is followed by a wait for ESP_BLE_MESH_CFG_CLIENT_*_STATUS via
 * the BLEMESH_MSG_CFG_EVT marshaled in blemesh_provisioner.c.
 */

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_networking_api.h"

#include "blemesh_internal.h"

#define TAG "blemesh_cfg"

#define APP_KEY_IDX  0x0000
#define NET_KEY_IDX  0x0000

#ifndef CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES
#define CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES 3
#endif

typedef enum {
    STEP_IDLE = 0,
    STEP_COMP_DATA_GET,
    STEP_APPKEY_ADD,
    STEP_MODEL_BIND,
    STEP_PUB_SET,
    STEP_HEARTBEAT_PUB,
    STEP_DEFAULT_TTL,
    STEP_DONE,
} cfg_step_t;

static const char *step_name(cfg_step_t s)
{
    switch (s) {
    case STEP_IDLE:          return "IDLE";
    case STEP_COMP_DATA_GET: return "COMP_DATA_GET";
    case STEP_APPKEY_ADD:    return "APPKEY_ADD";
    case STEP_MODEL_BIND:    return "MODEL_BIND";
    case STEP_PUB_SET:       return "PUB_SET";
    case STEP_HEARTBEAT_PUB: return "HEARTBEAT_PUB";
    case STEP_DEFAULT_TTL:   return "DEFAULT_TTL";
    case STEP_DONE:          return "DONE";
    }
    return "?";
}

/* Opcode the given step expects echoed on its STATUS/TIMEOUT reply. Used to
 * reject stale replies left over from a previous step (e.g. a delayed
 * COMPOSITION_DATA_STATUS landing while we're already in APPKEY_ADD). */
static uint32_t step_expected_opcode(cfg_step_t s)
{
    switch (s) {
    case STEP_COMP_DATA_GET: return ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET;
    case STEP_APPKEY_ADD:    return ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD;
    case STEP_MODEL_BIND:    return ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND;
    case STEP_PUB_SET:       return ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET;
    case STEP_HEARTBEAT_PUB: return ESP_BLE_MESH_MODEL_OP_HEARTBEAT_PUB_SET;
    case STEP_DEFAULT_TTL:   return ESP_BLE_MESH_MODEL_OP_DEFAULT_TTL_SET;
    default:                 return 0;
    }
}

/* One row per model discovered in the Composition Data. We record
 * the element offset (0 for the primary, 1 for the next, …) so MODEL_APP_BIND
 * and MODEL_PUB_SET target the right element address, and the company_id so
 * vendor models bind correctly (SIG models use ESP_BLE_MESH_CID_NVAL).
 *
 * needs_bind: true iff the manager needs an AppKey bound to this model — either
 *   because it will send commands to it (state-controlling server models) or
 *   receive publications from it (same set for SIG models, plus vendor models).
 *   Scene / Time / Scheduler / Setup-server variants and client models are
 *   excluded: the manager never talks to them, and binding them wastes on-air
 *   time and occasionally causes the leaf to timeout (e.g. Scene Server often
 *   delays its MODEL_APP_BIND_STATUS reply).
 *
 * needs_pub: true iff the manager also wants the model to publish its Status
 *   to the uplink group address.  Always a subset of needs_bind.  Vendor
 *   models are excluded — they publish autonomously on their own schedule. */
typedef struct {
    uint16_t element_offset;
    uint16_t model_id;
    uint16_t company_id;
    bool     needs_bind;
    bool     needs_pub;
} cfg_server_model_t;

/* The set of SIG server models the manager actually interacts with.
 * Used for both the BIND decision and the PUB_SET decision. */
static bool model_is_relevant(uint16_t model_id)
{
    switch (model_id) {
    case ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV:
    case ESP_BLE_MESH_MODEL_ID_GEN_LEVEL_SRV:
    case ESP_BLE_MESH_MODEL_ID_LIGHT_LIGHTNESS_SRV:
    case ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SRV:
    case ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_TEMP_SRV:
    case ESP_BLE_MESH_MODEL_ID_LIGHT_HSL_SRV:
    case ESP_BLE_MESH_MODEL_ID_LIGHT_HSL_HUE_SRV:
    case ESP_BLE_MESH_MODEL_ID_LIGHT_HSL_SAT_SRV:
    case ESP_BLE_MESH_MODEL_ID_SENSOR_SRV:
        return true;
    default:
        return false;
    }
}

/* Bind: relevant SIG server models AND vendor models (needed for their publications). */
static bool model_needs_bind(uint16_t model_id, uint16_t company_id)
{
    if (company_id != ESP_BLE_MESH_CID_NVAL) {
        return true; /* vendor models must be bound to publish with the AppKey */
    }
    return model_is_relevant(model_id);
}

/* Pub: only the state-publishing SIG server models; not vendor models. */
static bool model_needs_pub(uint16_t model_id, uint16_t company_id)
{
    if (company_id != ESP_BLE_MESH_CID_NVAL) {
        return false;
    }
    return model_is_relevant(model_id);
}

/* Sized for the richest leaf the manager expects to provision. The IDF
 * onoff_server example, when built as a full HSL+CTL light, exposes
 * 14 server models (8 on the primary element + 2 on each of the three
 * sibling elements for HSL Hue, HSL Saturation, and CTL Temperature).
 * 24 leaves comfortable headroom for vendor models and per-channel
 * extensions without making s_cfg blow up. */
#define CFG_MAX_SERVER_MODELS 24

static struct {
    cfg_step_t                step;
    blemesh_addr_t            addr;
    uint8_t                   uuid[16];
    blemesh_device_profile_t  profile;
    /* Server models discovered during Composition Data parse — populated
     * lazily; iterated by STEP_MODEL_BIND / STEP_PUB_SET. */
    cfg_server_model_t        servers[CFG_MAX_SERVER_MODELS];
    size_t                    server_model_count;
    size_t                    bind_cursor;
    uint8_t                   element_count;
    /* Per-step retry budget. Reset to CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES at
     * the start of every step / cursor advance; decremented each time the
     * leaf fails to reply (Config Client TIMEOUT_EVT, signalled by status=-1).
     * Hitting zero is a hard abort. STEP_MODEL_BIND / STEP_PUB_SET treat each
     * cursor position as its own step so a flaky model doesn't burn the
     * budget for its successors. */
    uint8_t                   step_retries_left;
} s_cfg;

/* ---------- Helpers ------------------------------------------------------ */

/* MODEL_APP_BIND is a small, single-segment message. Using the same 10 s
 * timeout as PUB_SET or HEARTBEAT_PUB means a single missed STATUS reply
 * burns 10 s before the retry fires. 4 s is enough for any well-behaved
 * leaf while still tolerating a busy vendor-model processing window. */
#define CFG_BIND_TIMEOUT_MS 4000

static esp_err_t send_cfg_with_timeout(blemesh_addr_t addr, uint32_t opcode,
                                       esp_ble_mesh_cfg_client_set_state_t *set,
                                       esp_ble_mesh_cfg_client_get_state_t *get,
                                       uint32_t timeout_ms)
{
    esp_ble_mesh_client_common_param_t common = {
        .opcode       = opcode,
        .model        = g_blemesh_ctx.models.config_client,
        .ctx          = {
            .net_idx  = NET_KEY_IDX,
            .app_idx  = APP_KEY_IDX,
            .addr     = addr,
            .send_ttl = g_blemesh_ctx.cfg.default_ttl,
        },
        .msg_timeout  = timeout_ms,
    };
    esp_err_t err;
    if (set) {
        err = esp_ble_mesh_config_client_set_state(&common, set);
    } else {
        err = esp_ble_mesh_config_client_get_state(&common, get);
    }
    return (err == ESP_OK) ? ESP_OK : ESP_FAIL;
}

static esp_err_t send_cfg(blemesh_addr_t addr, uint32_t opcode,
                          esp_ble_mesh_cfg_client_set_state_t *set,
                          esp_ble_mesh_cfg_client_get_state_t *get)
{
    return send_cfg_with_timeout(addr, opcode, set, get,
                                 g_blemesh_ctx.cfg.set_confirm_timeout_ms);
}

/* ---------- Composition Data parser (page 0) ----------------------------- */

static blemesh_device_profile_t classify_from_models(const cfg_server_model_t *m, size_t n)
{
    bool onoff = false, level = false, lightness = false, ctl = false, hsl = false;
    bool sensor = false;
    bool vendor = false;
    for (size_t i = 0; i < n; i++) {
        if (m[i].company_id != ESP_BLE_MESH_CID_NVAL) {
            vendor = true;
            continue;
        }
        switch (m[i].model_id) {
        case ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV:   onoff     = true; break;
        case ESP_BLE_MESH_MODEL_ID_GEN_LEVEL_SRV:   level     = true; break;
        case ESP_BLE_MESH_MODEL_ID_LIGHT_LIGHTNESS_SRV: lightness = true; break;
        case ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SRV:   ctl       = true; break;
        case ESP_BLE_MESH_MODEL_ID_LIGHT_HSL_SRV:   hsl       = true; break;
        case ESP_BLE_MESH_MODEL_ID_SENSOR_SRV:      sensor    = true; break;
        default: break;
        }
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
        return BLEMESH_DEV_TEMPERATURE;    /* coarse: refined later via property reads */
    }
    if (vendor) {
        return BLEMESH_DEV_VENDOR;
    }
    return BLEMESH_DEV_UNKNOWN;
}

static void parse_composition_data(const uint8_t *data, size_t len)
{
    /* Composition Data Page 0 layout:
     *   CID(2) PID(2) VID(2) CRPL(2) Features(2)
     *   per-element: Loc(2) NumS(1) NumV(1) [SIG model ids ×2] [vendor model ids ×4]
     *
     * For each server model we record (element_offset, model_id, company_id)
     * so MODEL_APP_BIND / MODEL_PUB_SET target the right element on
     * multi-element nodes (e.g. IDF's 3-element onoff_server example).
     */
    if (len < 10) {
        return;
    }
    size_t off = 10;
    s_cfg.server_model_count = 0;
    s_cfg.element_count      = 0;
    uint16_t element_offset  = 0;
    while (off + 4 <= len) {
        uint8_t nums = data[off + 2];
        uint8_t numv = data[off + 3];
        off += 4;
        for (uint8_t i = 0; i < nums && off + 2 <= len; i++, off += 2) {
            uint16_t id = (uint16_t)(data[off] | ((uint16_t)data[off + 1] << 8));
            /* Skip Foundation models — they use DevKey, not AppKey, so
             * AppKey-Bind / Pub-Set don't apply (would yield CannotBind). */
            if (id == ESP_BLE_MESH_MODEL_ID_CONFIG_SRV ||
                    id == ESP_BLE_MESH_MODEL_ID_CONFIG_CLI ||
                    id == ESP_BLE_MESH_MODEL_ID_HEALTH_SRV ||
                    id == ESP_BLE_MESH_MODEL_ID_HEALTH_CLI) {
                continue;
            }
            if (s_cfg.server_model_count < CFG_MAX_SERVER_MODELS) {
                s_cfg.servers[s_cfg.server_model_count].element_offset = element_offset;
                s_cfg.servers[s_cfg.server_model_count].model_id       = id;
                s_cfg.servers[s_cfg.server_model_count].company_id     = ESP_BLE_MESH_CID_NVAL;
                s_cfg.servers[s_cfg.server_model_count].needs_bind     =
                    model_needs_bind(id, ESP_BLE_MESH_CID_NVAL);
                s_cfg.servers[s_cfg.server_model_count].needs_pub      =
                    model_needs_pub(id, ESP_BLE_MESH_CID_NVAL);
                s_cfg.server_model_count++;
            }
        }
        for (uint8_t j = 0; j < numv && off + 4 <= len; j++, off += 4) {
            uint16_t cid = (uint16_t)(data[off]     | ((uint16_t)data[off + 1] << 8));
            uint16_t id  = (uint16_t)(data[off + 2] | ((uint16_t)data[off + 3] << 8));
            if (s_cfg.server_model_count < CFG_MAX_SERVER_MODELS) {
                s_cfg.servers[s_cfg.server_model_count].element_offset = element_offset;
                s_cfg.servers[s_cfg.server_model_count].model_id       = id;
                s_cfg.servers[s_cfg.server_model_count].company_id     = cid;
                s_cfg.servers[s_cfg.server_model_count].needs_bind     = true;  /* vendor — bind for pub rx */
                s_cfg.servers[s_cfg.server_model_count].needs_pub      = false; /* vendor — no pub cfg */
                s_cfg.server_model_count++;
            }
        }
        s_cfg.element_count++;
        element_offset++;
    }
    s_cfg.profile = classify_from_models(s_cfg.servers, s_cfg.server_model_count);
}

/* Derive the logical-element offset list from the parsed server-model table:
 * every element carrying the profile's primary SIG model is its own logical
 * element. Composite-light channel elements (CTL Temperature, HSL
 * Hue/Saturation) carry different model ids so they are excluded and collapse
 * into the primary logical element. Falls back to a single logical element at
 * offset 0. */
static void compute_logical_elem_offsets(uint8_t *offsets, uint8_t *count)
{
    uint16_t primary = blemesh_primary_model_for_profile(s_cfg.profile);
    *count = 0;
    if (primary != 0) {
        for (size_t i = 0; i < s_cfg.server_model_count; i++) {
            if (s_cfg.servers[i].company_id != ESP_BLE_MESH_CID_NVAL ||
                    s_cfg.servers[i].model_id != primary) {
                continue;
            }
            uint8_t off = (uint8_t)s_cfg.servers[i].element_offset;
            bool dup = false;
            for (uint8_t k = 0; k < *count; k++) {
                if (offsets[k] == off) {
                    dup = true;
                    break;
                }
            }
            if (!dup && *count < BLEMESH_MAX_LOGICAL_ELEMS_PER_NODE) {
                offsets[(*count)++] = off;
            }
        }
    }
    if (*count == 0) {
        offsets[0] = 0;
        *count     = 1;
    }
}

/* ---------- State machine ------------------------------------------------ */

/* Small FIFO of pending configurations — only one configuration runs at a
 * time, but the stack can finish multiple provisionings in quick succession. */
#define CFG_QUEUE_DEPTH 8
typedef struct {
    blemesh_addr_t addr;
    uint8_t uuid[16];
} cfg_pending_t;
static cfg_pending_t s_pending[CFG_QUEUE_DEPTH];
static uint8_t       s_pending_head;
static uint8_t       s_pending_tail;

static void advance(void);
static void start_next_pending(void);

bool blemesh_configurator_busy(void)
{
    return s_cfg.step != STEP_IDLE ||
           s_pending_head != s_pending_tail;
}

static void start_one(blemesh_addr_t addr, const uint8_t uuid[16])
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.addr = addr;
    memcpy(s_cfg.uuid, uuid, 16);
    s_cfg.step = STEP_COMP_DATA_GET;
    s_cfg.step_retries_left = CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES;
    advance();
}

void blemesh_configurator_start(blemesh_addr_t addr, const uint8_t uuid[16])
{
    if (s_cfg.step != STEP_IDLE) {
        /* Busy — enqueue. */
        uint8_t next = (uint8_t)((s_pending_tail + 1) % CFG_QUEUE_DEPTH);
        if (next == s_pending_head) {
            ESP_LOGE(TAG, "config queue full, dropping 0x%04x", addr);
            return;
        }
        s_pending[s_pending_tail].addr = addr;
        memcpy(s_pending[s_pending_tail].uuid, uuid, 16);
        s_pending_tail = next;
        ESP_LOGI(TAG, "queued config for 0x%04x (busy with 0x%04x)",
                 addr, s_cfg.addr);
        return;
    }
    start_one(addr, uuid);
}

static void start_next_pending(void)
{
    if (s_pending_head == s_pending_tail) {
        /* No more pending nodes — release the provisioning slot so the
         * stack callback will accept the next unprovisioned beacon. */
        g_blemesh_ctx.provisioning_busy = false;
        return;
    }
    cfg_pending_t p = s_pending[s_pending_head];
    s_pending_head = (uint8_t)((s_pending_head + 1) % CFG_QUEUE_DEPTH);
    start_one(p.addr, p.uuid);
}

/* The mesh stack's segmented-TX pool is small (CONFIG_BLE_MESH_TX_SEG_MSG_COUNT).
 * When configuration aborts the most common trigger is that we just tried
 * to send a multi-segment Config Client message and the stack came back
 * with -EBUSY / "No multi-segment message contexts available". If we then
 * immediately fire NODE_RESET (which is itself a segmented message), the
 * pool is still exhausted and the Reset gets dropped on the floor —
 * leaving the leaf provisioned-from-its-own-perspective and orphaned in
 * the network. Wait long enough for the in-flight segs to time out and
 * release their slots before we send NODE_RESET. */
#define ABORT_NODE_RESET_PRE_DELAY_MS  600

static void abort_configuration(const char *reason)
{
    ESP_LOGE(TAG,
             "configuration aborted for 0x%04x: %s (step=%s cursor=%zu/%zu)",
             s_cfg.addr, reason, step_name(s_cfg.step),
             s_cfg.bind_cursor, s_cfg.server_model_count);
    /* Tell the node to factory-reset so it doesn't end up orphaned (still
     * provisioned from its own perspective, but unknown to us). The node
     * may not respond — we delete locally regardless. */
    if (g_blemesh_ctx.models.config_client) {
        /* Drain time. We're running on the manager task; the segmented-TX
         * pool is drained by the BTU task, so a vTaskDelay here just
         * yields. Other manager-task work queues up behind us — that's
         * acceptable on the abort path. */
        vTaskDelay(pdMS_TO_TICKS(ABORT_NODE_RESET_PRE_DELAY_MS));

        esp_ble_mesh_client_common_param_t common = {
            .opcode       = ESP_BLE_MESH_MODEL_OP_NODE_RESET,
            .model        = g_blemesh_ctx.models.config_client,
            .ctx.net_idx  = NET_KEY_IDX,
            .ctx.app_idx  = APP_KEY_IDX,
            .ctx.addr     = s_cfg.addr,
            .ctx.send_ttl = g_blemesh_ctx.cfg.default_ttl,
            .msg_timeout  = g_blemesh_ctx.cfg.set_confirm_timeout_ms,
        };
        esp_ble_mesh_cfg_client_set_state_t reset_state = { 0 };
        esp_err_t reset_err =
            esp_ble_mesh_config_client_set_state(&common, &reset_state);
        if (reset_err != ESP_OK) {
            ESP_LOGW(TAG,
                     "node_reset send to 0x%04x failed: %s — leaf will be "
                     "orphaned and need manual erase-flash",
                     s_cfg.addr, esp_err_to_name(reset_err));
        } else {
            ESP_LOGI(TAG, "node_reset 0x%04x dispatched", s_cfg.addr);
        }
    }
    (void)esp_ble_mesh_provisioner_delete_node_with_addr(s_cfg.addr);
    /* If STEP_DONE had already inserted into the directory (timeout arrived
     * late), clean that up too and notify the consumer. */
    if (blemesh_dir_remove(s_cfg.addr) == ESP_OK && g_blemesh_ctx.cb.on_node_removed) {
        g_blemesh_ctx.cb.on_node_removed(s_cfg.addr);
    }
    blemesh_pending_clear_all_for(s_cfg.addr);
    s_cfg.step = STEP_IDLE;
    start_next_pending();
}

static void advance(void)
{
    switch (s_cfg.step) {

    case STEP_COMP_DATA_GET: {
        esp_ble_mesh_cfg_client_get_state_t get = {
            .comp_data_get = { .page = 0x00 },
        };
        ESP_LOGI(TAG, "cfg 0x%04x %s (attempt %u/%u)",
                 s_cfg.addr, step_name(s_cfg.step),
                 (unsigned)(CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES + 1 - s_cfg.step_retries_left),
                 (unsigned)(CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES + 1));
        if (send_cfg(s_cfg.addr, ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET,
                     NULL, &get) != ESP_OK) {
            abort_configuration("comp_data_get send failed");
        }
        break;
    }

    case STEP_APPKEY_ADD: {
        esp_ble_mesh_cfg_client_set_state_t set = {
            .app_key_add = {
                .net_idx = NET_KEY_IDX,
                .app_idx = APP_KEY_IDX,
            },
        };
        memcpy(set.app_key_add.app_key, g_blemesh_ctx.cfg.app_key, 16);
        ESP_LOGI(TAG, "cfg 0x%04x %s (attempt %u/%u)",
                 s_cfg.addr, step_name(s_cfg.step),
                 (unsigned)(CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES + 1 - s_cfg.step_retries_left),
                 (unsigned)(CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES + 1));
        if (send_cfg(s_cfg.addr, ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD,
                     &set, NULL) != ESP_OK) {
            abort_configuration("appkey_add send failed");
        }
        break;
    }

    case STEP_MODEL_BIND: {
        /* Skip models that the manager never talks to (Scene, Time, Scheduler,
         * Setup-server variants, client models). Binding them wastes on-air
         * time and may cause the leaf to delay or drop the STATUS reply. */
        while (s_cfg.bind_cursor < s_cfg.server_model_count &&
                !s_cfg.servers[s_cfg.bind_cursor].needs_bind) {
            s_cfg.bind_cursor++;
        }
        if (s_cfg.bind_cursor >= s_cfg.server_model_count) {
            s_cfg.step = STEP_PUB_SET;
            s_cfg.bind_cursor = 0;
            s_cfg.step_retries_left = CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES;
            advance();
            return;
        }
        const cfg_server_model_t *sm = &s_cfg.servers[s_cfg.bind_cursor];
        esp_ble_mesh_cfg_client_set_state_t set = {
            .model_app_bind = {
                .element_addr  = (uint16_t)(s_cfg.addr + sm->element_offset),
                .model_app_idx = APP_KEY_IDX,
                .model_id      = sm->model_id,
                .company_id    = sm->company_id,
            },
        };
        ESP_LOGI(TAG,
                 "cfg 0x%04x BIND[%zu/%zu] elem=0x%04x model=0x%04x cid=0x%04x (attempt %u/%u)",
                 s_cfg.addr, s_cfg.bind_cursor + 1, s_cfg.server_model_count,
                 set.model_app_bind.element_addr, sm->model_id, sm->company_id,
                 (unsigned)(CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES + 1 - s_cfg.step_retries_left),
                 (unsigned)(CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES + 1));
        if (send_cfg_with_timeout(s_cfg.addr, ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND,
                                  &set, NULL, CFG_BIND_TIMEOUT_MS) != ESP_OK) {
            abort_configuration("model_app_bind send failed");
        }
        break;
    }

    case STEP_PUB_SET: {
        /* Skip models that don't need a publication address configured. */
        while (s_cfg.bind_cursor < s_cfg.server_model_count &&
                !s_cfg.servers[s_cfg.bind_cursor].needs_pub) {
            s_cfg.bind_cursor++;
        }
        if (s_cfg.bind_cursor >= s_cfg.server_model_count) {
            s_cfg.step = STEP_HEARTBEAT_PUB;
            s_cfg.step_retries_left = CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES;
            advance();
            return;
        }
        const cfg_server_model_t *sm = &s_cfg.servers[s_cfg.bind_cursor];
        esp_ble_mesh_cfg_client_set_state_t set = {
            .model_pub_set = {
                .element_addr        = (uint16_t)(s_cfg.addr + sm->element_offset),
                .publish_addr        = g_blemesh_ctx.cfg.uplink_group_addr,
                .publish_app_idx     = APP_KEY_IDX,
                .cred_flag           = false,
                .publish_ttl         = g_blemesh_ctx.cfg.default_ttl,
                .publish_period      = 0x00,   /* event-driven only */
                /* No model-level retransmits — network layer already
                 * retransmits at the bearer. Avoids fan-out of duplicate
                 * Status publications at the provisioner. */
                .publish_retransmit  = ESP_BLE_MESH_PUBLISH_TRANSMIT(0, 50),
                .model_id            = sm->model_id,
                .company_id          = sm->company_id,
            },
        };
        ESP_LOGI(TAG,
                 "cfg 0x%04x PUB[%zu/%zu] elem=0x%04x model=0x%04x cid=0x%04x (attempt %u/%u)",
                 s_cfg.addr, s_cfg.bind_cursor + 1, s_cfg.server_model_count,
                 set.model_pub_set.element_addr, sm->model_id, sm->company_id,
                 (unsigned)(CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES + 1 - s_cfg.step_retries_left),
                 (unsigned)(CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES + 1));
        if (send_cfg(s_cfg.addr, ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET,
                     &set, NULL) != ESP_OK) {
            abort_configuration("model_pub_set send failed");
        }
        break;
    }

    case STEP_HEARTBEAT_PUB: {
        /* Derive Heartbeat Publication Period Log from configured period_ms:
         * actual period = 2^(N-1) seconds, valid N is 1..17. */
        uint32_t period_s = g_blemesh_ctx.cfg.heartbeat_period_ms / 1000U;
        if (period_s == 0) {
            period_s = 1;
        }
        uint8_t  period_log = 1;
        while ((1U << (period_log - 1)) < period_s && period_log < 17) {
            period_log++;
        }
        esp_ble_mesh_cfg_client_set_state_t set = {
            .heartbeat_pub_set = {
                .dst       = g_blemesh_ctx.cfg.prov_addr_start,
                .count     = 0xFF,        /* continuous */
                .period    = period_log,
                .ttl       = g_blemesh_ctx.cfg.default_ttl,
                .feature   = ESP_BLE_MESH_FEATURE_RELAY |
                ESP_BLE_MESH_FEATURE_PROXY |
                ESP_BLE_MESH_FEATURE_FRIEND |
                ESP_BLE_MESH_FEATURE_LOW_POWER,
                .net_idx   = NET_KEY_IDX,
            },
        };
        ESP_LOGI(TAG, "cfg 0x%04x %s (attempt %u/%u)",
                 s_cfg.addr, step_name(s_cfg.step),
                 (unsigned)(CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES + 1 - s_cfg.step_retries_left),
                 (unsigned)(CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES + 1));
        if (send_cfg(s_cfg.addr, ESP_BLE_MESH_MODEL_OP_HEARTBEAT_PUB_SET,
                     &set, NULL) != ESP_OK) {
            abort_configuration("heartbeat_pub_set send failed");
        }
        break;
    }

    case STEP_DEFAULT_TTL: {
        esp_ble_mesh_cfg_client_set_state_t set = {
            .default_ttl_set = { .ttl = g_blemesh_ctx.cfg.default_ttl },
        };
        ESP_LOGI(TAG, "cfg 0x%04x %s (attempt %u/%u)",
                 s_cfg.addr, step_name(s_cfg.step),
                 (unsigned)(CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES + 1 - s_cfg.step_retries_left),
                 (unsigned)(CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES + 1));
        if (send_cfg(s_cfg.addr, ESP_BLE_MESH_MODEL_OP_DEFAULT_TTL_SET,
                     &set, NULL) != ESP_OK) {
            abort_configuration("default_ttl_set send failed");
        }
        break;
    }

    case STEP_DONE: {
        blemesh_node_info_t info = {
            .addr          = s_cfg.addr,
            .profile       = s_cfg.profile,
            .element_count = s_cfg.element_count,
            .company_id    = 0xFFFF,
        };
        compute_logical_elem_offsets(info.logical_elem_offset, &info.logical_elem_count);
        /* Record the Light CTL Temperature element so temperature-only changes
         * can target it directly (see blemesh_node_info_t.ctl_temp_offset). */
        for (size_t si = 0; si < s_cfg.server_model_count; si++) {
            if (s_cfg.servers[si].company_id != ESP_BLE_MESH_CID_NVAL) {
                continue;
            }
            uint8_t eo = (uint8_t)s_cfg.servers[si].element_offset;
            switch (s_cfg.servers[si].model_id) {
            case ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_TEMP_SRV:
                if (info.ctl_temp_offset == 0) {
                    info.ctl_temp_offset = eo;
                }
                break;
            case ESP_BLE_MESH_MODEL_ID_LIGHT_HSL_HUE_SRV:
                if (info.hsl_hue_offset == 0) {
                    info.hsl_hue_offset = eo;
                }
                break;
            case ESP_BLE_MESH_MODEL_ID_LIGHT_HSL_SAT_SRV:
                if (info.hsl_sat_offset == 0) {
                    info.hsl_sat_offset = eo;
                }
                break;
            default:
                break;
            }
        }
        /* Render UUID as canonical hex string. */
        static const char hex[] = "0123456789abcdef";
        char *p = info.uuid;
        for (int i = 0; i < 16; i++) {
            *p++ = hex[(s_cfg.uuid[i] >> 4) & 0xF];
            *p++ = hex[s_cfg.uuid[i] & 0xF];
            if (i == 3 || i == 5 || i == 7 || i == 9) {
                *p++ = '-';
            }
        }
        *p = '\0';

        blemesh_node_entry_t *e = blemesh_dir_insert(&info);
        if (e == NULL) {
            abort_configuration("directory full");
            return;
        }
        if (g_blemesh_ctx.cb.on_node_provisioned) {
            g_blemesh_ctx.cb.on_node_provisioned(&info, true);
        }
        /* Kick off paced initial-state Gets only for elements that have a
         * model the manager can query. We know which elements are relevant from
         * the server-model list we just built: any element whose offset appears
         * in at least one entry with needs_pub=true has a state-publishing SIG
         * server model the manager can query, so a Get to that element will be
         * answered. Elements that only carry Scene/Scheduler/vendor-only models
         * would silently time out (no state-publishing SIG server to respond). */
        bool element_has_server[CFG_MAX_SERVER_MODELS] = {false};
        for (size_t si = 0; si < s_cfg.server_model_count; si++) {
            if (s_cfg.servers[si].needs_pub &&
                    s_cfg.servers[si].element_offset < CFG_MAX_SERVER_MODELS) {
                element_has_server[s_cfg.servers[si].element_offset] = true;
            }
        }
        uint8_t sync_elems = info.element_count < CFG_MAX_SERVER_MODELS
                             ? info.element_count : CFG_MAX_SERVER_MODELS;
        for (uint8_t ei = 0; ei < sync_elems; ei++) {
            if (element_has_server[ei]) {
                blemesh_initial_sync_enqueue(
                    (blemesh_addr_t)(info.addr + ei), info.profile);
            }
        }
        s_cfg.step = STEP_IDLE;
        start_next_pending();
        break;
    }

    case STEP_IDLE:
    default:
        break;
    }
}

void blemesh_configurator_on_reply(const blemesh_msg_t *msg)
{
    if (msg->u.cfg.addr != s_cfg.addr || s_cfg.step == STEP_IDLE) {
        return;
    }
    /* Reject stale/duplicate replies. The opcode must match the request the
     * current step issued — this drops a delayed STATUS left over from a
     * previous step. For the per-model BIND/PUB steps every reply carries the
     * same opcode, so also match the echoed element+model against the cursor
     * we're waiting on: without this a delayed STATUS plus the resend's STATUS
     * both bind_cursor++ and silently skip a model. Element/model are only
     * valid on real status events (status != -1), not on TIMEOUT_EVT. */
    if (msg->u.cfg.opcode != step_expected_opcode(s_cfg.step)) {
        ESP_LOGW(TAG, "cfg 0x%04x stale reply op=0x%06" PRIx32 " in %s — ignored",
                 s_cfg.addr, msg->u.cfg.opcode, step_name(s_cfg.step));
        return;
    }
    if (msg->u.cfg.status == 0 &&
            (s_cfg.step == STEP_MODEL_BIND || s_cfg.step == STEP_PUB_SET) &&
            s_cfg.bind_cursor < s_cfg.server_model_count) {
        const cfg_server_model_t *sm = &s_cfg.servers[s_cfg.bind_cursor];
        uint16_t want_elem = (uint16_t)(s_cfg.addr + sm->element_offset);
        if (msg->u.cfg.elem_addr != want_elem ||
                msg->u.cfg.model_id != sm->model_id) {
            ESP_LOGW(TAG,
                     "cfg 0x%04x stale %s status elem=0x%04x model=0x%04x "
                     "(want elem=0x%04x model=0x%04x) — ignored",
                     s_cfg.addr, step_name(s_cfg.step),
                     msg->u.cfg.elem_addr, msg->u.cfg.model_id,
                     want_elem, sm->model_id);
            return;
        }
    }
    /* TIMEOUT_EVT is marshaled as status=-1 (the actual Config Client API
     * does not pass an error code on timeout). For dropped STATUS replies on
     * rich multi-element lights — 14 binds + 14 pub-sets in a single burst,
     * radio potentially shared with other BLE activity — re-send the same transaction up to
     * CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES times before tearing the node down.
     * A non-zero positive error_code is a hard failure (model not found,
     * invalid address, etc.) and shouldn't be retried because the leaf
     * actively rejected the request. */
    if (msg->u.cfg.status != 0) {
        bool is_timeout = (msg->u.cfg.status == -1);
        if (is_timeout && s_cfg.step_retries_left > 0) {
            s_cfg.step_retries_left--;
            ESP_LOGW(TAG,
                     "cfg 0x%04x %s timeout @ cursor=%zu — retrying (%u left)",
                     s_cfg.addr, step_name(s_cfg.step), s_cfg.bind_cursor,
                     s_cfg.step_retries_left);
            advance();
            return;
        }
        abort_configuration(is_timeout
                            ? "cfg client timeout (retries exhausted)"
                            : "cfg client reply error");
        return;
    }

    /* Successful reply — advance to the next sub-step and refresh the retry
     * budget for it. STEP_MODEL_BIND and STEP_PUB_SET treat each cursor
     * position as its own sub-step (so a flaky model only burns its own
     * budget, not the next ones'). */
    switch (s_cfg.step) {
    case STEP_COMP_DATA_GET:
        if (msg->u.cfg.data && msg->u.cfg.len > 0) {
            parse_composition_data(msg->u.cfg.data, msg->u.cfg.len);
            /* Persist the raw Composition Data so it survives reboot — the
             * stack stores it in NVS keyed by node addr, and we read it back
             * via esp_ble_mesh_provisioner_get_node_with_addr() on restore. */
            (void)esp_ble_mesh_provisioner_store_node_comp_data(
                s_cfg.addr, msg->u.cfg.data, (uint16_t)msg->u.cfg.len);
            ESP_LOGI(TAG,
                     "cfg 0x%04x parsed comp: elements=%u server_models=%zu profile=%d",
                     s_cfg.addr, s_cfg.element_count, s_cfg.server_model_count,
                     (int)s_cfg.profile);
        }
        s_cfg.step              = STEP_APPKEY_ADD;
        s_cfg.step_retries_left = CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES;
        advance();
        break;

    case STEP_APPKEY_ADD:
        s_cfg.step              = STEP_MODEL_BIND;
        s_cfg.bind_cursor       = 0;
        s_cfg.step_retries_left = CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES;
        advance();
        break;

    case STEP_MODEL_BIND:
        s_cfg.bind_cursor++;
        s_cfg.step_retries_left = CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES;
        advance();
        break;

    case STEP_PUB_SET:
        s_cfg.bind_cursor++;
        s_cfg.step_retries_left = CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES;
        advance();
        break;

    case STEP_HEARTBEAT_PUB:
        s_cfg.step              = STEP_DEFAULT_TTL;
        s_cfg.step_retries_left = CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES;
        advance();
        break;

    case STEP_DEFAULT_TTL:
        s_cfg.step              = STEP_DONE;
        s_cfg.step_retries_left = CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES;
        advance();
        break;

    default:
        break;
    }
}
