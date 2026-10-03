/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tests for the per-destination Get tracking. The client Get APIs are
 * wrapped at link time, so no mesh stack and no peer node are necessary.
 */

#include <errno.h>
#include <string.h>

#include "unity.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_lighting_model_api.h"

#include "blemesh_internal.h"

#define ADDR_A           0x0010
#define ADDR_B           0x0020
#define ADDR_GROUP       0xC000
#define TEST_TIMEOUT_MS  100
#define SLOT_SLACK_MS    300

extern void blemesh_generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event,
                                      esp_ble_mesh_generic_client_cb_param_t *param);

static int      s_send_count;
static uint16_t s_last_dst;
static uint32_t s_last_opcode;

esp_err_t __wrap_esp_ble_mesh_generic_client_get_state(esp_ble_mesh_client_common_param_t *params,
                                                       esp_ble_mesh_generic_client_get_state_t *get_state)
{
    (void)get_state;
    s_send_count++;
    s_last_dst    = params->ctx.addr;
    s_last_opcode = params->opcode;
    return ESP_OK;
}

esp_err_t __wrap_esp_ble_mesh_light_client_get_state(esp_ble_mesh_client_common_param_t *params,
                                                     esp_ble_mesh_light_client_get_state_t *get_state)
{
    (void)get_state;
    s_send_count++;
    s_last_dst    = params->ctx.addr;
    s_last_opcode = params->opcode;
    return ESP_OK;
}

static void drain_queue(void)
{
    blemesh_msg_t m;
    while (xQueueReceive(g_blemesh_ctx.queue, &m, 0) == pdTRUE) {
        blemesh_free_msg(&m);
    }
}

static void get_setup(void)
{
    static int s_dummy_model;
    if (g_blemesh_ctx.queue == NULL) {
        g_blemesh_ctx.queue = xQueueCreate(8, sizeof(blemesh_msg_t));
        TEST_ASSERT_NOT_NULL(g_blemesh_ctx.queue);
    }
    g_blemesh_ctx.models.generic_onoff_client   = (esp_ble_mesh_model_t *)&s_dummy_model;
    g_blemesh_ctx.models.light_lightness_client = (esp_ble_mesh_model_t *)&s_dummy_model;
    g_blemesh_ctx.cfg.set_confirm_timeout_ms    = TEST_TIMEOUT_MS;
    TEST_ASSERT_EQUAL(ESP_OK, blemesh_dispatcher_init());
    blemesh_initial_sync_clear_all();
    drain_queue();
    s_send_count  = 0;
    s_last_dst    = 0;
    s_last_opcode = 0;
}

static void get_teardown(void)
{
    blemesh_initial_sync_clear_all();
    TEST_ASSERT_EQUAL(ESP_OK, blemesh_dispatcher_init());
    drain_queue();
}

TEST_CASE("get to a destination with a pending Get is refused until it completes", "[blemesh][get_track]")
{
    get_setup();
    TEST_ASSERT_EQUAL(ESP_OK, blemesh_dispatcher_send_get(ADDR_A, BLEMESH_STATE_ONOFF));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FINISHED, blemesh_dispatcher_send_get(ADDR_A, BLEMESH_STATE_LIGHTNESS));
    TEST_ASSERT_EQUAL(1, s_send_count);

    TEST_ASSERT_EQUAL(ESP_OK, blemesh_dispatcher_send_get(ADDR_B, BLEMESH_STATE_ONOFF));
    TEST_ASSERT_EQUAL(2, s_send_count);

    blemesh_dispatcher_on_get_done(ADDR_A, 0);
    TEST_ASSERT_EQUAL(ESP_OK, blemesh_dispatcher_send_get(ADDR_A, BLEMESH_STATE_LIGHTNESS));
    TEST_ASSERT_EQUAL(3, s_send_count);
    TEST_ASSERT_EQUAL_HEX16(ADDR_A, s_last_dst);
    TEST_ASSERT_EQUAL_HEX32(ESP_BLE_MESH_MODEL_OP_LIGHT_LIGHTNESS_GET, s_last_opcode);
    get_teardown();
}

TEST_CASE("get send error frees the destination", "[blemesh][get_track]")
{
    get_setup();
    TEST_ASSERT_EQUAL(ESP_OK, blemesh_dispatcher_send_get(ADDR_A, BLEMESH_STATE_ONOFF));
    blemesh_dispatcher_on_get_done(ADDR_A, -ENOMEM);
    TEST_ASSERT_EQUAL(ESP_OK, blemesh_dispatcher_send_get(ADDR_A, BLEMESH_STATE_ONOFF));
    TEST_ASSERT_EQUAL(2, s_send_count);
    get_teardown();
}

TEST_CASE("get to a group address is not tracked", "[blemesh][get_track]")
{
    get_setup();
    TEST_ASSERT_EQUAL(ESP_OK, blemesh_dispatcher_send_get(ADDR_GROUP, BLEMESH_STATE_ONOFF));
    TEST_ASSERT_EQUAL(ESP_OK, blemesh_dispatcher_send_get(ADDR_GROUP, BLEMESH_STATE_ONOFF));
    TEST_ASSERT_EQUAL(2, s_send_count);
    get_teardown();
}

TEST_CASE("get slot expires when no completion event arrives", "[blemesh][get_track]")
{
    get_setup();
    TEST_ASSERT_EQUAL(ESP_OK, blemesh_dispatcher_send_get(ADDR_A, BLEMESH_STATE_ONOFF));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FINISHED, blemesh_dispatcher_send_get(ADDR_A, BLEMESH_STATE_ONOFF));
    vTaskDelay(pdMS_TO_TICKS(TEST_TIMEOUT_MS + 2000 + SLOT_SLACK_MS));
    TEST_ASSERT_EQUAL(ESP_OK, blemesh_dispatcher_send_get(ADDR_A, BLEMESH_STATE_ONOFF));
    TEST_ASSERT_EQUAL(2, s_send_count);
    get_teardown();
}

TEST_CASE("sync tick skips a busy destination and sends the next entry", "[blemesh][get_track]")
{
    get_setup();
    TEST_ASSERT_EQUAL(ESP_OK, blemesh_dispatcher_send_get(ADDR_A, BLEMESH_STATE_ONOFF));
    blemesh_initial_sync_get(ADDR_A, BLEMESH_STATE_LIGHTNESS);
    blemesh_initial_sync_enqueue(ADDR_B, BLEMESH_DEV_ONOFF);
    TEST_ASSERT_EQUAL(1, s_send_count);

    blemesh_initial_sync_on_tick();
    TEST_ASSERT_EQUAL(2, s_send_count);
    TEST_ASSERT_EQUAL_HEX16(ADDR_B, s_last_dst);

    /* Every entry is blocked: the tick sends nothing. */
    blemesh_initial_sync_on_tick();
    TEST_ASSERT_EQUAL(2, s_send_count);

    blemesh_dispatcher_on_get_done(ADDR_A, 0);
    blemesh_initial_sync_on_tick();
    TEST_ASSERT_EQUAL(3, s_send_count);
    TEST_ASSERT_EQUAL_HEX16(ADDR_A, s_last_dst);
    TEST_ASSERT_EQUAL_HEX32(ESP_BLE_MESH_MODEL_OP_LIGHT_LIGHTNESS_GET, s_last_opcode);
    get_teardown();
}

TEST_CASE("get EBUSY re-queues the Get until the destination is free", "[blemesh][get_track]")
{
    get_setup();
    TEST_ASSERT_EQUAL(ESP_OK, blemesh_dispatcher_send_get(ADDR_A, BLEMESH_STATE_ONOFF));
    blemesh_dispatcher_on_get_done(ADDR_A, -EBUSY);

    blemesh_initial_sync_on_tick();
    TEST_ASSERT_EQUAL(1, s_send_count);

    blemesh_dispatcher_on_get_done(ADDR_A, 0);
    blemesh_initial_sync_on_tick();
    TEST_ASSERT_EQUAL(2, s_send_count);
    TEST_ASSERT_EQUAL_HEX16(ADDR_A, s_last_dst);
    TEST_ASSERT_EQUAL_HEX32(ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET, s_last_opcode);
    get_teardown();
}

static void client_event(esp_ble_mesh_generic_client_cb_event_t event, int error)
{
    esp_ble_mesh_client_common_param_t common = {
        .opcode   = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET,
        .ctx.addr = ADDR_A,
    };
    esp_ble_mesh_generic_client_cb_param_t param = {
        .error_code = error,
        .params     = &common,
    };
    blemesh_generic_client_cb(event, &param);
}

static void expect_get_done(int error)
{
    blemesh_msg_t m;
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(g_blemesh_ctx.queue, &m, 0));
    TEST_ASSERT_EQUAL(BLEMESH_MSG_GET_DONE, m.kind);
    TEST_ASSERT_EQUAL_HEX16(ADDR_A, m.u.get_done.dst);
    TEST_ASSERT_EQUAL(error, m.u.get_done.error);
}

TEST_CASE("client timeout and send error post only a Get completion", "[blemesh][get_track]")
{
    get_setup();
    blemesh_msg_t m;

    client_event(ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT, 0);
    expect_get_done(0);
    TEST_ASSERT_EQUAL(pdFALSE, xQueueReceive(g_blemesh_ctx.queue, &m, 0));

    client_event(ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT, -EBUSY);
    expect_get_done(-EBUSY);
    TEST_ASSERT_EQUAL(pdFALSE, xQueueReceive(g_blemesh_ctx.queue, &m, 0));

    client_event(ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT, 0);
    expect_get_done(0);
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(g_blemesh_ctx.queue, &m, 0));
    TEST_ASSERT_EQUAL(BLEMESH_MSG_RX_STATUS, m.kind);
    blemesh_free_msg(&m);

    client_event(ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT, 0);
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(g_blemesh_ctx.queue, &m, 0));
    TEST_ASSERT_EQUAL(BLEMESH_MSG_RX_STATUS, m.kind);
    blemesh_free_msg(&m);
    get_teardown();
}

static int                   s_state_count;
static blemesh_addr_t        s_state_addr;
static blemesh_state_value_t s_state_val;

static void on_state_changed(blemesh_addr_t addr, const blemesh_state_value_t *val)
{
    s_state_count++;
    s_state_addr = addr;
    s_state_val  = *val;
}

TEST_CASE("get reply decodes the Status opcode and fires on_state_changed", "[blemesh][get_track]")
{
    get_setup();
    void (*saved_cb)(blemesh_addr_t, const blemesh_state_value_t *) = g_blemesh_ctx.cb.on_state_changed;
    g_blemesh_ctx.cb.on_state_changed = on_state_changed;
    s_state_count = 0;

    esp_ble_mesh_client_common_param_t common = {
        .opcode      = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET,
        .ctx.addr    = ADDR_A,
        .ctx.recv_op = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
    };
    esp_ble_mesh_generic_client_cb_param_t param = {
        .params = &common,
        .status_cb.onoff_status.present_onoff = 1,
    };
    blemesh_generic_client_cb(ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT, &param);
    expect_get_done(0);

    blemesh_msg_t m;
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(g_blemesh_ctx.queue, &m, 0));
    TEST_ASSERT_EQUAL(BLEMESH_MSG_RX_STATUS, m.kind);
    TEST_ASSERT_EQUAL_HEX32(ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS, m.u.rx_status.opcode);
    blemesh_publication_rx_dispatch(&m);
    blemesh_free_msg(&m);

    g_blemesh_ctx.cb.on_state_changed = saved_cb;
    TEST_ASSERT_EQUAL(1, s_state_count);
    TEST_ASSERT_EQUAL_HEX16(ADDR_A, s_state_addr);
    TEST_ASSERT_EQUAL(BLEMESH_STATE_ONOFF, s_state_val.id);
    TEST_ASSERT_TRUE(s_state_val.v.onoff);
    get_teardown();
}
