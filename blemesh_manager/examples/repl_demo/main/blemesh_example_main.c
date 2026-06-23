/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file blemesh_example_main.c
 * @brief Minimal demo of blemesh_manager. Provides:
 *          - BLE stack + ESP-BLE-MESH provisioner composition setup,
 *          - registration of Client models with the manager,
 *          - callbacks that log every event,
 *          - an esp_console REPL exposing the manager's public API.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_bt.h"
#include "nvs_flash.h"
#include "argtable3/argtable3.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef CONFIG_BT_BLUEDROID_ENABLED
#include "esp_bt_main.h"
#endif
#ifdef CONFIG_BT_NIMBLE_ENABLED
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#endif

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_lighting_model_api.h"
#include "esp_ble_mesh_sensor_model_api.h"

#include "blemesh_manager.h"

#define TAG "blemesh_ex"

/* ============================================================================
 * Provisioner composition
 * ========================================================================== */

#define PROV_OWN_ADDR_START   0x0001
#define PROV_OWN_ADDR_COUNT   0x0010
#define UPLINK_GROUP_ADDR     0xC000
#define NET_KEY_IDX           0x0000
#define APP_KEY_IDX           0x0000

static uint8_t s_dev_uuid[16] = { 0xdd, 0xdd };
static uint8_t s_net_key[16]  = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                  0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
                                };
static uint8_t s_app_key[16]  = { 0x10, 0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09,
                                  0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01
                                };

/* Per-client publication buffers. */
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_cli_pub, 2 + 1, ROLE_PROVISIONER);
ESP_BLE_MESH_MODEL_PUB_DEFINE(level_cli_pub, 2 + 5, ROLE_PROVISIONER);
ESP_BLE_MESH_MODEL_PUB_DEFINE(lightness_cli_pub, 2 + 5, ROLE_PROVISIONER);
ESP_BLE_MESH_MODEL_PUB_DEFINE(ctl_cli_pub, 2 + 9, ROLE_PROVISIONER);
ESP_BLE_MESH_MODEL_PUB_DEFINE(hsl_cli_pub, 2 + 9, ROLE_PROVISIONER);
ESP_BLE_MESH_MODEL_PUB_DEFINE(sensor_cli_pub, 2 + 8, ROLE_PROVISIONER);

static esp_ble_mesh_client_t onoff_client;
static esp_ble_mesh_client_t level_client;
static esp_ble_mesh_client_t lightness_client;
static esp_ble_mesh_client_t ctl_client;
static esp_ble_mesh_client_t hsl_client;
static esp_ble_mesh_client_t sensor_client;

static esp_ble_mesh_cfg_srv_t config_server = {
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
    .default_ttl = 7,
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};

static esp_ble_mesh_client_t cfg_client;

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_CFG_CLI(&cfg_client),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_cli_pub, &onoff_client),
    ESP_BLE_MESH_MODEL_GEN_LEVEL_CLI(&level_cli_pub, &level_client),
    ESP_BLE_MESH_MODEL_LIGHT_LIGHTNESS_CLI(&lightness_cli_pub, &lightness_client),
    ESP_BLE_MESH_MODEL_LIGHT_CTL_CLI(&ctl_cli_pub, &ctl_client),
    ESP_BLE_MESH_MODEL_LIGHT_HSL_CLI(&hsl_cli_pub, &hsl_client),
    ESP_BLE_MESH_MODEL_SENSOR_CLI(&sensor_cli_pub, &sensor_client),
};

#define CFG_CLI_IDX       1
#define ONOFF_CLI_IDX     2
#define LEVEL_CLI_IDX     3
#define LIGHTNESS_CLI_IDX 4
#define CTL_CLI_IDX       5
#define HSL_CLI_IDX       6
#define SENSOR_CLI_IDX    7

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, ESP_BLE_MESH_MODEL_NONE),
};

static esp_ble_mesh_comp_t composition = {
    .cid = 0x02E5, /* Espressif CID */
    .elements = elements,
    .element_count = ARRAY_SIZE(elements),
};

static esp_ble_mesh_prov_t provision = {
    .prov_uuid          = s_dev_uuid,
    .prov_unicast_addr  = PROV_OWN_ADDR_START,
    .prov_start_address = PROV_OWN_ADDR_START + PROV_OWN_ADDR_COUNT,
};

/* ============================================================================
 * Manager callbacks — log everything
 * ========================================================================== */

static const char *profile_str(blemesh_device_profile_t p)
{
    switch (p) {
    case BLEMESH_DEV_ONOFF:       return "OnOff";
    case BLEMESH_DEV_DIMMABLE:    return "Dimmable";
    case BLEMESH_DEV_COLOR_TEMP:  return "ColorTemp";
    case BLEMESH_DEV_COLOR_HSL:   return "ColorHSL";
    case BLEMESH_DEV_OCCUPANCY:   return "Occupancy";
    case BLEMESH_DEV_TEMPERATURE: return "Temperature";
    case BLEMESH_DEV_VENDOR:      return "Vendor";
    default:                      return "Unknown";
    }
}

static const char *state_str(blemesh_state_id_t s)
{
    switch (s) {
    case BLEMESH_STATE_ONOFF:        return "onoff";
    case BLEMESH_STATE_LEVEL:        return "level";
    case BLEMESH_STATE_LIGHTNESS:    return "lightness";
    case BLEMESH_STATE_CTL:          return "ctl";
    case BLEMESH_STATE_HSL:          return "hsl";
    case BLEMESH_STATE_SENSOR_VALUE: return "sensor";
    default:                         return "?";
    }
}

static void on_node_provisioned(const blemesh_node_info_t *info, bool is_fresh)
{
    ESP_LOGI(TAG, "[EVT] node provisioned addr=0x%04x profile=%s elem=%u %s uuid=%s",
             info->addr, profile_str(info->profile), info->element_count,
             is_fresh ? "(fresh)" : "(restored)", info->uuid);
}

static void on_node_removed(blemesh_addr_t addr)
{
    ESP_LOGI(TAG, "[EVT] node removed addr=0x%04x", addr);
}

static void on_reachability(blemesh_addr_t addr, bool reachable)
{
    ESP_LOGI(TAG, "[EVT] node 0x%04x reachable=%d", addr, (int)reachable);
}

static void on_state_changed(blemesh_addr_t addr, const blemesh_state_value_t *val)
{
    switch (val->id) {
    case BLEMESH_STATE_ONOFF:
        ESP_LOGI(TAG, "[EVT] state change: addr=0x%04x onoff=%d", addr, (int)val->v.onoff);
        break;
    case BLEMESH_STATE_LEVEL:
        ESP_LOGI(TAG, "[EVT] state change: addr=0x%04x level=%d", addr, (int)val->v.level);
        break;
    case BLEMESH_STATE_LIGHTNESS:
        ESP_LOGI(TAG, "[EVT] state change: addr=0x%04x lightness=%u", addr, val->v.lightness);
        break;
    case BLEMESH_STATE_CTL:
        ESP_LOGI(TAG, "[EVT] state change: addr=0x%04x ctl lightness=%u temp=%u",
                 addr, val->v.ctl.lightness, val->v.ctl.temperature);
        break;
    case BLEMESH_STATE_HSL:
        ESP_LOGI(TAG, "[EVT] state change: addr=0x%04x hsl l=%u h=%u s=%u",
                 addr, val->v.hsl.l, val->v.hsl.h, val->v.hsl.s);
        break;
    case BLEMESH_STATE_SENSOR_VALUE:
        ESP_LOGI(TAG, "[EVT] state change: addr=0x%04x sensor prop=0x%04x len=%u",
                 addr, val->v.sensor.property_id, val->v.sensor.len);
        ESP_LOG_BUFFER_HEX(TAG, val->v.sensor.data, val->v.sensor.len);
        break;
    default:
        ESP_LOGI(TAG, "[EVT] state change: addr=0x%04x state=%d (unhandled)", addr, (int)val->id);
        break;
    }
}

static void on_set_failed(blemesh_addr_t addr, blemesh_state_id_t state)
{
    ESP_LOGW(TAG, "[EVT] SET FAILED addr=0x%04x state=%s", addr, state_str(state));
}

static void on_vendor_message(blemesh_addr_t addr, uint16_t cid, uint32_t opcode,
                              const uint8_t *data, size_t len)
{
    ESP_LOGI(TAG, "[EVT] vendor msg 0x%04x cid=0x%04x opcode=0x%08lx len=%zu",
             addr, cid, (unsigned long)opcode, len);
    if (data && len) {
        ESP_LOG_BUFFER_HEX(TAG, data, len);
    }
}

/* ============================================================================
 * Console commands — public API only
 * ========================================================================== */

static int cmd_nodes(int argc, char **argv)
{
    (void)argc; (void)argv;
    size_t count = 0;
    blemesh_manager_list_nodes(NULL, &count);
    if (count == 0) {
        printf("(no nodes)\n");
        return 0;
    }
    blemesh_node_info_t *buf = calloc(count, sizeof(*buf));
    if (!buf) {
        printf("oom\n");
        return 1;
    }
    blemesh_manager_list_nodes(buf, &count);
    printf("addr    profile      elem  reach  uuid\n");
    printf("------- ------------ ----  -----  ------------------------------------\n");
    for (size_t i = 0; i < count; i++) {
        printf("0x%04x  %-12s %4u  %-5s  %s\n",
               buf[i].addr, profile_str(buf[i].profile),
               buf[i].element_count,
               buf[i].reachable ? "yes" : "no",
               buf[i].uuid);
    }
    free(buf);
    return 0;
}

static struct {
    struct arg_str *onoff;
    struct arg_int *seconds;
    struct arg_end *end;
} commission_args;

static int cmd_commission(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&commission_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, commission_args.end, argv[0]);
        return 1;
    }
    bool enable = strcmp(commission_args.onoff->sval[0], "on") == 0;
    uint32_t window = (commission_args.seconds->count > 0)
                      ? (uint32_t)commission_args.seconds->ival[0]
                      : 60;
    esp_err_t err = blemesh_manager_enable_commissioning(enable, window);
    printf("commissioning %s (window=%lus) -> %s\n",
           enable ? "ON" : "OFF", (unsigned long)window, esp_err_to_name(err));
    return 0;
}

static struct {
    struct arg_int *addr;
    struct arg_str *state;
    struct arg_str *value;
    struct arg_int *transition;
    struct arg_end *end;
} set_args;

static blemesh_state_id_t parse_state_id(const char *s, bool *ok)
{
    *ok = true;
    if (!strcmp(s, "onoff")) {
        return BLEMESH_STATE_ONOFF;
    }
    if (!strcmp(s, "level")) {
        return BLEMESH_STATE_LEVEL;
    }
    if (!strcmp(s, "lightness")) {
        return BLEMESH_STATE_LIGHTNESS;
    }
    if (!strcmp(s, "ctl")) {
        return BLEMESH_STATE_CTL;
    }
    if (!strcmp(s, "hsl")) {
        return BLEMESH_STATE_HSL;
    }
    if (!strcmp(s, "sensor")) {
        return BLEMESH_STATE_SENSOR_VALUE;
    }
    *ok = false;
    return BLEMESH_STATE_ONOFF;
}

static int cmd_set(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&set_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_args.end, argv[0]);
        return 1;
    }
    bool ok = false;
    blemesh_state_id_t state = parse_state_id(set_args.state->sval[0], &ok);
    if (!ok) {
        printf("bad state. use: onoff|level|lightness|ctl|hsl\n");
        return 1;
    }
    blemesh_state_value_t v = { .id = state };
    const char *raw = set_args.value->sval[0];
    switch (state) {
    case BLEMESH_STATE_ONOFF:
        v.v.onoff = (strcmp(raw, "on") == 0 || strcmp(raw, "1") == 0 ||
                     strcmp(raw, "true") == 0);
        break;
    case BLEMESH_STATE_LEVEL:
        v.v.level = (int16_t)strtol(raw, NULL, 0);
        break;
    case BLEMESH_STATE_LIGHTNESS:
        v.v.lightness = (uint16_t)strtoul(raw, NULL, 0);
        break;
    case BLEMESH_STATE_CTL: {
        /* "<lightness>,<temp>" */
        char buf[32];
        strncpy(buf, raw, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
        char *comma = strchr(buf, ',');
        if (!comma) {
            printf("ctl value: <lightness>,<temp>\n");
            return 1;
        }
        *comma = 0;
        v.v.ctl.lightness   = (uint16_t)strtoul(buf, NULL, 0);
        v.v.ctl.temperature = (uint16_t)strtoul(comma + 1, NULL, 0);
        break;
    }
    case BLEMESH_STATE_HSL: {
        /* "<l>,<h>,<s>" */
        char buf[40];
        strncpy(buf, raw, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
        char *p1 = strchr(buf, ',');
        char *p2 = p1 ? strchr(p1 + 1, ',') : NULL;
        if (!p1 || !p2) {
            printf("hsl value: <l>,<h>,<s>\n");
            return 1;
        }
        *p1 = 0; *p2 = 0;
        v.v.hsl.l = (uint16_t)strtoul(buf, NULL, 0);
        v.v.hsl.h = (uint16_t)strtoul(p1 + 1, NULL, 0);
        v.v.hsl.s = (uint16_t)strtoul(p2 + 1, NULL, 0);
        break;
    }
    default:
        printf("state not settable via this command\n");
        return 1;
    }
    uint32_t trans = (set_args.transition->count > 0)
                     ? (uint32_t)set_args.transition->ival[0] : 0;
    esp_err_t err = blemesh_manager_set_state((blemesh_addr_t)set_args.addr->ival[0],
                    &v, trans);
    printf("set -> %s\n", esp_err_to_name(err));
    return 0;
}

static struct {
    struct arg_int *addr;
    struct arg_str *state;
    struct arg_end *end;
} get_args;

static int cmd_get(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&get_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, get_args.end, argv[0]);
        return 1;
    }
    bool ok = false;
    blemesh_state_id_t state = parse_state_id(get_args.state->sval[0], &ok);
    if (!ok) {
        printf("bad state\n");
        return 1;
    }
    esp_err_t err = blemesh_manager_get_state((blemesh_addr_t)get_args.addr->ival[0],
                    state);
    printf("get -> %s\n", esp_err_to_name(err));
    return 0;
}

static struct {
    struct arg_int *addr;
    struct arg_end *end;
} remove_args;

static int cmd_remove(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&remove_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, remove_args.end, argv[0]);
        return 1;
    }
    esp_err_t err = blemesh_manager_remove_node((blemesh_addr_t)remove_args.addr->ival[0]);
    printf("remove -> %s\n", esp_err_to_name(err));
    return 0;
}

static int cmd_reset(int argc, char **argv)
{
    (void)argc; (void)argv;
    esp_err_t err = blemesh_manager_reset();
    printf("reset -> %s\n", esp_err_to_name(err));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "rebooting in 2 seconds");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }
    return 0;
}

static void register_console_commands(void)
{
    const esp_console_cmd_t nodes_cmd = {
        .command = "nodes",
        .help    = "List all provisioned nodes",
        .func    = &cmd_nodes,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&nodes_cmd));

    commission_args.onoff   = arg_str1(NULL, NULL, "<on|off>", "Enable/disable commissioning");
    commission_args.seconds = arg_int0("t", "time", "<sec>", "Auto-close window (default 60)");
    commission_args.end     = arg_end(2);
    const esp_console_cmd_t commission_cmd = {
        .command = "commission",
        .help    = "Open or close the commissioning window",
        .argtable = &commission_args,
        .func    = &cmd_commission,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&commission_cmd));

    set_args.addr       = arg_int1(NULL, NULL, "<addr>",
                                   "Node unicast address (decimal or 0xHEX)");
    set_args.state      = arg_str1(NULL, NULL, "<state>",
                                   "onoff|level|lightness|ctl|hsl");
    set_args.value      = arg_str1(NULL, NULL, "<value>",
                                   "on|off|0|1, int, or comma-list per state");
    set_args.transition = arg_int0("t", "trans", "<ms>", "Transition time in ms");
    set_args.end        = arg_end(4);
    const esp_console_cmd_t set_cmd = {
        .command  = "set",
        .help     = "Send a Set to a node",
        .argtable = &set_args,
        .func     = &cmd_set,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_cmd));

    get_args.addr  = arg_int1(NULL, NULL, "<addr>", "Node unicast address");
    get_args.state = arg_str1(NULL, NULL, "<state>", "onoff|level|lightness|ctl|hsl|sensor");
    get_args.end   = arg_end(2);
    const esp_console_cmd_t get_cmd = {
        .command  = "get",
        .help     = "Send a Get to a node",
        .argtable = &get_args,
        .func     = &cmd_get,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&get_cmd));

    remove_args.addr = arg_int1(NULL, NULL, "<addr>", "Node unicast address");
    remove_args.end  = arg_end(1);
    const esp_console_cmd_t remove_cmd = {
        .command  = "remove",
        .help     = "Remove a node from the network",
        .argtable = &remove_args,
        .func     = &cmd_remove,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&remove_cmd));

    const esp_console_cmd_t reset_cmd = {
        .command = "reset",
        .help    = "Factory-reset the manager",
        .func    = &cmd_reset,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&reset_cmd));

    ESP_ERROR_CHECK(esp_console_register_help_command());
}

/* ============================================================================
 * BLE + mesh + manager bring-up
 * ========================================================================== */

#ifdef CONFIG_BT_NIMBLE_ENABLED
/* NimBLE host bring-up. esp_ble_mesh_init() needs the host running and synced
 * first, so we block on a sync semaphore the host signals once its address is
 * resolved. Mirrors IDF's ble_mesh_example_init common component. */
static SemaphoreHandle_t s_mesh_sem;
void ble_store_config_init(void);   /* provided by the NimBLE host store */

static void mesh_on_reset(int reason)
{
    ESP_LOGI(TAG, "nimble host reset; reason=%d", reason);
}

static void mesh_on_sync(void)
{
    /* Make sure we have a usable identity address before mesh starts. */
    (void)ble_hs_util_ensure_addr(0);
    xSemaphoreGive(s_mesh_sem);
}

static void mesh_host_task(void *param)
{
    (void)param;
    ESP_LOGI(TAG, "nimble host task started");
    nimble_port_run();                  /* returns only on nimble_port_stop() */
    nimble_port_freertos_deinit();
}
#endif /* CONFIG_BT_NIMBLE_ENABLED */

static void bluetooth_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

#ifdef CONFIG_BT_BLUEDROID_ENABLED
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
#endif

#ifdef CONFIG_BT_NIMBLE_ENABLED
    s_mesh_sem = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(s_mesh_sem ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.reset_cb        = mesh_on_reset;
    ble_hs_cfg.sync_cb         = mesh_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();
    nimble_port_freertos_init(mesh_host_task);
    /* Wait for the host to sync before returning — the manager calls
     * esp_ble_mesh_init() right after this. */
    xSemaphoreTake(s_mesh_sem, portMAX_DELAY);
#endif
}

void app_main(void)
{
    ESP_LOGI(TAG, "starting blemesh_manager example");

    /* Bring up NVS + BT controller + Bluedroid here (typically shared with
     * other components). The manager owns esp_ble_mesh_init and everything
     * mesh-related after that. */
    bluetooth_init();

    blemesh_manager_config_t cfg = {
        .prov                      = &provision,
        .comp                      = &composition,
        .prov_addr_start           = PROV_OWN_ADDR_START,
        .prov_addr_count           = PROV_OWN_ADDR_COUNT,
        .uplink_group_addr         = UPLINK_GROUP_ADDR,
        .heartbeat_period_ms       = 30000,
        .heartbeat_loss_threshold  = 3,
        /* Governs Config Client + Get/Set confirmation deadlines (the manager
         * passes this, not CONFIG_BLE_MESH_CLIENT_MSG_TIMEOUT). 10 s tolerates
         * Composition Data Get under radio contention with many nodes. */
        .set_confirm_timeout_ms    = 10000,
        .set_retries               = 1,
        .default_ttl               = 7,
    };
    memcpy(cfg.net_key, s_net_key, sizeof(cfg.net_key));
    memcpy(cfg.app_key, s_app_key, sizeof(cfg.app_key));
    ESP_ERROR_CHECK(blemesh_manager_init(&cfg));

    blemesh_manager_callbacks_t cb = {
        .on_node_provisioned = on_node_provisioned,
        .on_node_removed     = on_node_removed,
        .on_reachability     = on_reachability,
        .on_state_changed    = on_state_changed,
        .on_set_failed       = on_set_failed,
        .on_vendor_message   = on_vendor_message,
    };
    ESP_ERROR_CHECK(blemesh_manager_register_callbacks(&cb));

    blemesh_manager_models_t models = {
        .config_client          = &root_models[CFG_CLI_IDX],
        .generic_onoff_client   = &root_models[ONOFF_CLI_IDX],
        .generic_level_client   = &root_models[LEVEL_CLI_IDX],
        .light_lightness_client = &root_models[LIGHTNESS_CLI_IDX],
        .light_ctl_client       = &root_models[CTL_CLI_IDX],
        .light_hsl_client       = &root_models[HSL_CLI_IDX],
        .sensor_client          = &root_models[SENSOR_CLI_IDX],
        .vendor_client          = NULL,
    };
    ESP_ERROR_CHECK(blemesh_manager_register_models(&models));

    ESP_ERROR_CHECK(blemesh_manager_start());
    ESP_LOGI(TAG, "manager started; provisioner addr=0x%04x", PROV_OWN_ADDR_START);

    /* Console */
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "blemesh> ";
    esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_cfg, &repl_cfg, &repl));

    register_console_commands();

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    ESP_LOGI(TAG, "console ready. type 'help' for commands.");
}
