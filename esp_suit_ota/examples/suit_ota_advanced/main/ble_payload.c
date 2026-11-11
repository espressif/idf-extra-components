/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/*
 * The second transport of the example.
 *
 * The device advertises a small GATT service with two characteristics. When the OTA loop needs a
 * component whose URI starts with "ble://", the getter writes that request into the request
 * characteristic and notifies the connected peer. The peer answers by writing the bytes of that
 * resource into the data characteristic, and the getter hands them back to esp_suit_ota_feed().
 *
 * The request is one line of text: "GET <uri> <offset> <length>". The offset is not always zero,
 * because a campaign that was cut by a reset resumes in the middle of a component.
 */
#include "sdkconfig.h"
#include "ble_payload.h"

#if CONFIG_EXAMPLE_ENABLE_BLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_suit_ota_err.h"

static const char *TAG = "ble_payload";

#define REQ_CAP    192
#define CHUNK_CAP  512

/* 3a2b1c0d-5e4f-6071-8293-a4b5c6d7000{1,2,3}: service, request, data */
static const ble_uuid128_t svc_uuid = BLE_UUID128_INIT(
                                          0x01, 0x00, 0xd7, 0xc6, 0xb5, 0xa4, 0x93, 0x82, 0x71, 0x60, 0x4f, 0x5e, 0x0d, 0x1c, 0x2b, 0x3a);
static const ble_uuid128_t req_uuid = BLE_UUID128_INIT(
                                          0x02, 0x00, 0xd7, 0xc6, 0xb5, 0xa4, 0x93, 0x82, 0x71, 0x60, 0x4f, 0x5e, 0x0d, 0x1c, 0x2b, 0x3a);
static const ble_uuid128_t data_uuid = BLE_UUID128_INIT(
                                           0x03, 0x00, 0xd7, 0xc6, 0xb5, 0xa4, 0x93, 0x82, 0x71, 0x60, 0x4f, 0x5e, 0x0d, 0x1c, 0x2b, 0x3a);

/* one of these per getter, owned by the getter and freed by close() */
typedef struct {
    uint32_t epoch;        /* the resource this state last asked for */
    size_t delivered;      /* bytes handed to the core for that resource */
} ble_getter_state_t;

static StreamBufferHandle_t s_rx;
static uint16_t s_req_handle;
static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
static uint8_t s_own_addr_type;
static char s_req[REQ_CAP];
static bool s_running;
static volatile bool s_overflow;

static void start_advertising(void);

static int req_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    return os_mbuf_append(ctxt->om, s_req, strlen(s_req)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int data_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    uint8_t chunk[CHUNK_CAP];
    uint16_t len = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, chunk, sizeof(chunk), &len) != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (xStreamBufferSend(s_rx, chunk, len, pdMS_TO_TICKS(200)) != len) {
        s_overflow = true;      /* the peer writes faster than the OTA loop reads */
    }
    return 0;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[])
        {
            {
                .uuid = &req_uuid.u,
                .access_cb = req_access,
                .val_handle = &s_req_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            }, {
                .uuid = &data_uuid.u,
                .access_cb = data_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { 0 },
        },
    },
    { 0 },
};

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn = event->connect.conn_handle;
            ESP_LOGI(TAG, "payload peer connected");
        } else {
            start_advertising();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        s_conn = BLE_HS_CONN_HANDLE_NONE;
        ESP_LOGW(TAG, "payload peer disconnected");
        start_advertising();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        break;
    default:
        break;
    }
    return 0;
}

static void start_advertising(void)
{
    const char *name = ble_svc_gap_device_name();
    struct ble_hs_adv_fields fields = { 0 };
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)name;
    fields.name_len = (uint8_t)strlen(name);
    fields.name_is_complete = 1;
    if (ble_gap_adv_set_fields(&fields) != 0) {
        ESP_LOGE(TAG, "the advertising data was rejected");
        return;
    }
    struct ble_gap_adv_params adv = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };
    int rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv, gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "advertising failed: %d", rc);
    }
}

static void on_sync(void)
{
    if (ble_hs_id_infer_auto(0, &s_own_addr_type) != 0) {
        ESP_LOGE(TAG, "no usable BLE address");
        return;
    }
    s_running = true;
    start_advertising();
    ESP_LOGI(TAG, "advertising as '%s', a ble:// component waits for this peer", ble_svc_gap_device_name());
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t example_ble_payload_start(void)
{
    s_rx = xStreamBufferCreate(CONFIG_EXAMPLE_BLE_RX_BUF, 1);
    if (s_rx == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
        return err;
    }
    ble_hs_cfg.sync_cb = on_sync;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc == 0) {
        rc = ble_gatts_add_svcs(gatt_svcs);
    }
    if (rc == 0) {
        rc = ble_svc_gap_device_name_set(CONFIG_EXAMPLE_BLE_DEVICE_NAME);
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT setup failed: %d", rc);
        return ESP_FAIL;
    }
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

static void publish_request(const char *url, size_t offset, size_t len)
{
    snprintf(s_req, sizeof(s_req), "GET %s %u %u", url, (unsigned)offset, (unsigned)len);
    ESP_LOGI(TAG, "request to the peer: %s", s_req);
    if (s_conn == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGW(TAG, "no peer is connected yet; connect one to answer this request");
        return;
    }
    ble_gatts_chr_updated(s_req_handle);
}

int example_ble_payload_get(esp_suit_ota_transport_t *t, void **state, uint8_t *buf, size_t cap)
{
    ble_getter_state_t *st = *state;
    if (st == NULL) {
        st = calloc(1, sizeof(*st));
        if (st == NULL) {
            return -ESP_ERR_NO_MEM;
        }
        st->epoch = t->epoch - 1;          /* differs from t->epoch, so the first call asks */
        *state = st;
    }
    if (!s_running) {
        ESP_LOGE(TAG, "the BLE stack is not up");
        return -ESP_ERR_SUIT_OTA_NO_TRANSPORT;
    }
    if (st->epoch != t->epoch) {           /* the core moved to another resource */
        st->epoch = t->epoch;
        st->delivered = 0;
        s_overflow = false;
        xStreamBufferReset(s_rx);
        publish_request(t->url, t->offset, t->len);
    }
    if (t->len != ESP_SUIT_OTA_LEN_UNKNOWN && st->delivered >= t->len) {
        return 0;                          /* this resource is complete */
    }
    size_t want = cap;
    if (t->len != ESP_SUIT_OTA_LEN_UNKNOWN && want > t->len - st->delivered) {
        want = t->len - st->delivered;
    }
    size_t n = xStreamBufferReceive(s_rx, buf, want, pdMS_TO_TICKS(CONFIG_EXAMPLE_BLE_TIMEOUT_MS));
    if (s_overflow) {
        ESP_LOGE(TAG, "the peer wrote faster than the OTA loop could read");
        return -ESP_ERR_SUIT_OTA_INCOMPLETE;
    }
    if (n == 0) {
        ESP_LOGE(TAG, "the peer sent nothing for %d ms", CONFIG_EXAMPLE_BLE_TIMEOUT_MS);
        return -ESP_ERR_TIMEOUT;
    }
    st->delivered += n;
    return (int)n;
}

void example_ble_payload_close(void *state)
{
    free(state);
}

#endif /* CONFIG_EXAMPLE_ENABLE_BLE */
