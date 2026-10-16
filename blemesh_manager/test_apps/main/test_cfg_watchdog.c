/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tests for the configurator step watchdog. The Config Client send APIs are
 * wrapped at link time, so no mesh stack and no peer node are necessary.
 */

#include <string.h>

#include "unity.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_config_model_api.h"

#include "blemesh_internal.h"

#define TEST_ADDR        0x0005
#define TEST_TIMEOUT_MS  100
#define WD_PERIOD_MS     (TEST_TIMEOUT_MS + CONFIG_BLEMESH_MGR_CFG_STEP_WATCHDOG_MARGIN_S * 1000)
#define WD_SLACK_MS      300

static const uint8_t s_uuid[16] = { 0xAA };
static int           s_send_count;
static uint32_t      s_last_opcode;

esp_err_t __wrap_esp_ble_mesh_config_client_get_state(esp_ble_mesh_client_common_param_t *params,
                                                      esp_ble_mesh_cfg_client_get_state_t *get_state)
{
    (void)get_state;
    s_send_count++;
    s_last_opcode = params->opcode;
    return ESP_OK;
}

esp_err_t __wrap_esp_ble_mesh_config_client_set_state(esp_ble_mesh_client_common_param_t *params,
                                                      esp_ble_mesh_cfg_client_set_state_t *set_state)
{
    (void)set_state;
    s_send_count++;
    s_last_opcode = params->opcode;
    return ESP_OK;
}

esp_err_t __wrap_esp_ble_mesh_provisioner_delete_node_with_addr(uint16_t unicast_addr)
{
    (void)unicast_addr;
    return ESP_OK;
}

static void drain_queue(void)
{
    blemesh_msg_t m;
    while (xQueueReceive(g_blemesh_ctx.queue, &m, 0) == pdTRUE) {
        blemesh_free_msg(&m);
    }
}

static void wd_setup(void)
{
    static int s_dummy_model;
    if (g_blemesh_ctx.queue == NULL) {
        g_blemesh_ctx.queue = xQueueCreate(8, sizeof(blemesh_msg_t));
        TEST_ASSERT_NOT_NULL(g_blemesh_ctx.queue);
    }
    if (g_blemesh_ctx.cfg_wd_timer == NULL) {
        TEST_ASSERT_EQUAL(ESP_OK, blemesh_configurator_init());
    }
    g_blemesh_ctx.models.config_client     = (esp_ble_mesh_model_t *)&s_dummy_model;
    g_blemesh_ctx.cfg.set_confirm_timeout_ms = TEST_TIMEOUT_MS;
    drain_queue();
    s_send_count  = 0;
    s_last_opcode = 0;
}

/* A hard error reply aborts the node, which leaves the configurator idle for the next test. */
static void wd_teardown(void)
{
    if (blemesh_configurator_busy()) {
        blemesh_msg_t err = { .kind = BLEMESH_MSG_CFG_EVT };
        err.u.cfg.addr   = TEST_ADDR;
        err.u.cfg.opcode = s_last_opcode;
        err.u.cfg.status = 1;
        blemesh_configurator_on_reply(&err);
    }
    TEST_ASSERT_FALSE(blemesh_configurator_busy());
    drain_queue();
}

static void feed(uint32_t opcode, int status)
{
    blemesh_msg_t m = { .kind = BLEMESH_MSG_CFG_EVT };
    m.u.cfg.addr   = TEST_ADDR;
    m.u.cfg.opcode = opcode;
    m.u.cfg.status = status;
    blemesh_configurator_on_reply(&m);
}

static blemesh_msg_t wait_synthetic_timeout(void)
{
    blemesh_msg_t m;
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(g_blemesh_ctx.queue, &m,
                                            pdMS_TO_TICKS(WD_PERIOD_MS + WD_SLACK_MS)));
    TEST_ASSERT_EQUAL(BLEMESH_MSG_CFG_EVT, m.kind);
    TEST_ASSERT_EQUAL_HEX16(TEST_ADDR, m.u.cfg.addr);
    TEST_ASSERT_EQUAL(-1, m.u.cfg.status);
    TEST_ASSERT_NOT_EQUAL(0, m.u.cfg.wd_gen);
    return m;
}

TEST_CASE("cfg watchdog posts a timeout when no Config Client event arrives", "[blemesh][cfg_wd]")
{
    wd_setup();
    blemesh_configurator_start(TEST_ADDR, s_uuid);
    TEST_ASSERT_EQUAL(1, s_send_count);
    TEST_ASSERT_EQUAL_HEX32(ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET, s_last_opcode);

    blemesh_msg_t m;
    TEST_ASSERT_EQUAL(pdFALSE, xQueueReceive(g_blemesh_ctx.queue, &m,
                                             pdMS_TO_TICKS(WD_PERIOD_MS - WD_SLACK_MS)));
    m = wait_synthetic_timeout();
    TEST_ASSERT_EQUAL_HEX32(ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET, m.u.cfg.opcode);

    blemesh_configurator_on_reply(&m);
    TEST_ASSERT_EQUAL(2, s_send_count);
    TEST_ASSERT_EQUAL_HEX32(ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET, s_last_opcode);
    wd_teardown();
}

TEST_CASE("cfg watchdog timeout from an earlier arm is ignored", "[blemesh][cfg_wd]")
{
    wd_setup();
    blemesh_configurator_start(TEST_ADDR, s_uuid);
    blemesh_msg_t stale = wait_synthetic_timeout();

    feed(ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET, -1);
    TEST_ASSERT_EQUAL(2, s_send_count);

    blemesh_configurator_on_reply(&stale);
    TEST_ASSERT_EQUAL(2, s_send_count);
    wd_teardown();
}

TEST_CASE("cfg late reply after a watchdog timeout advances the step once", "[blemesh][cfg_wd]")
{
    wd_setup();
    blemesh_configurator_start(TEST_ADDR, s_uuid);
    blemesh_msg_t m = wait_synthetic_timeout();
    blemesh_configurator_on_reply(&m);
    TEST_ASSERT_EQUAL(2, s_send_count);

    feed(ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET, 0);
    TEST_ASSERT_EQUAL(3, s_send_count);
    TEST_ASSERT_EQUAL_HEX32(ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD, s_last_opcode);

    feed(ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET, 0);
    TEST_ASSERT_EQUAL(3, s_send_count);
    TEST_ASSERT_EQUAL_HEX32(ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD, s_last_opcode);
    wd_teardown();
}

TEST_CASE("cfg watchdog timeouts exhaust retries and clear provisioning_busy", "[blemesh][cfg_wd]")
{
    wd_setup();
    g_blemesh_ctx.provisioning_busy = true;
    blemesh_configurator_start(TEST_ADDR, s_uuid);

    for (int i = 0; i <= CONFIG_BLEMESH_MGR_CFG_STEP_RETRIES; i++) {
        blemesh_msg_t m = wait_synthetic_timeout();
        blemesh_configurator_on_reply(&m);
    }
    TEST_ASSERT_EQUAL_HEX32(ESP_BLE_MESH_MODEL_OP_NODE_RESET, s_last_opcode);
    TEST_ASSERT_FALSE(blemesh_configurator_busy());
    TEST_ASSERT_FALSE(g_blemesh_ctx.provisioning_busy);

    blemesh_msg_t m;
    TEST_ASSERT_EQUAL(pdFALSE, xQueueReceive(g_blemesh_ctx.queue, &m,
                                             pdMS_TO_TICKS(WD_PERIOD_MS + WD_SLACK_MS)));
    wd_teardown();
}
