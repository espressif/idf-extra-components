/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/*
 * Two transports in one campaign.
 *
 * The application fetches the manifest itself with esp_http_client and feeds the bytes to the
 * SUIT processor. The processor then works through one component at a time, and the loop pulls
 * those bytes through the transport context. That context holds the links this device has, and
 * each pair declares which one brings it, so the app arrives over HTTPS and the f1 table over BLE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include "esp_suit_ota.h"
#include "esp_suit_ota_helpers.h"
#include "esp_ota_slots.h"
#include "suit_identity.h"
#if CONFIG_EXAMPLE_ENABLE_BLE
#include "ble_payload.h"
#endif

static const char *TAG = "suit_advanced";

extern const uint8_t anchor_start[] asm("_binary_anchor_p256_bin_start");

#define BUF_SIZE 4096

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
#define SKIP_CN_CHECK true
#else
#define SKIP_CN_CHECK false
#endif

static esp_http_client_config_t s_http = {
#if CONFIG_EXAMPLE_USE_CERT_BUNDLE
    .crt_bundle_attach = esp_crt_bundle_attach,
#endif
    .skip_cert_common_name_check = SKIP_CN_CHECK,
    .timeout_ms = 10000,
    .keep_alive_enable = true,
};
static esp_suit_ota_https_cfg_t s_https = {
    .http_config = &s_http,
    .max_request_size = CONFIG_EXAMPLE_HTTP_REQUEST_SIZE,
};

/*
 * One row per pair: what it is and which partitions it owns.
 *
 * app and f1 flip together (group "cal"); cfg is a COPY pair written to cfg_stage and copied into
 * cfg at activation. The partition labels are free: nothing is derived from the pair name. The
 * manifest reaches these pairs by name, because the last part of a component identifier names its
 * pair, and it chooses the link by the scheme of each component's URI.
 */
static const esp_ota_slots_decl_t slots[] = {
    { .name = "app", .kind = ESP_OTA_SLOTS_APP, .group = "cal" },
    { .name = "f1",  .kind = ESP_OTA_SLOTS_AB,  .primary = "f1_live", .secondary = "f1_spare", .group = "cal" },
    { .name = "cfg", .kind = ESP_OTA_SLOTS_COPY, .primary = "cfg",     .secondary = "cfg_stage" },
};

static const esp_suit_ota_key_t keys[] = { { .point = anchor_start, .point_len = 65 } };
static const esp_suit_ota_config_t cfg = {
    .keys = keys,
    .num_keys = 1,
    .vendor_ids = &vendor_uuid,
    .num_vendor_ids = 1,
    .class_ids = &class_uuid,
    .num_class_ids = 1,
};

static void suit_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id) {
    case ESP_SUIT_OTA_EVENT_MANIFEST_VERIFIED: {
        esp_suit_ota_event_manifest_t *m = data;
        ESP_LOGI(TAG, "event: manifest %llu, %u components, %u to fetch", (unsigned long long)m->sequence_number,
                 m->num_components, m->num_pending);
        break;
    }
    case ESP_SUIT_OTA_EVENT_COMPONENT_NEXT: {
        esp_suit_ota_event_component_t *c = data;
        ESP_LOGI(TAG, "event: fetching %s (%lu bytes) from %s", c->id, (unsigned long)c->size, c->uri);
        break;
    }
    case ESP_SUIT_OTA_EVENT_COMPONENT_SKIPPED:
        ESP_LOGI(TAG, "event: %s already installed", ((esp_suit_ota_event_component_t *)data)->id);
        break;
    case ESP_SUIT_OTA_EVENT_COMPONENT_STAGED:
        ESP_LOGI(TAG, "event: %s staged", ((esp_suit_ota_event_component_t *)data)->id);
        break;
    case ESP_SUIT_OTA_EVENT_PROGRESS: {
        esp_suit_ota_event_progress_t *p = data;
        ESP_LOGI(TAG, "event: component %u at %lu/%lu", p->index, (unsigned long)p->bytes_written,
                 (unsigned long)p->size);
        break;
    }
    case ESP_SUIT_OTA_EVENT_ABORTED:
        ESP_LOGW(TAG, "event: aborted, 0x%x", ((esp_suit_ota_event_abort_t *)data)->err);
        break;
    default:
        break;
    }
}

/* the product's own check of new data: f1 and cfg carry a known signature, or are still blank */
static bool data_valid(void)
{
    uint8_t h[8];
    esp_partition_read(esp_ota_slots_get_active("f1"), 0, h, sizeof(h));
    bool f1_ok = memcmp(h, "F1-DATA", 7) == 0 || h[0] == 0xff;
    esp_partition_read(esp_ota_slots_get_active("cfg"), 0, h, sizeof(h));
    bool cfg_ok = memcmp(h, "CFG-", 4) == 0 || h[0] == 0xff;
    return f1_ok && cfg_ok;
}

static void after_activation(bool restart)
{
    if (restart) {
        ESP_LOGI(TAG, "RESULT: app release activated, restarting");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
    if (data_valid()) {
        ESP_ERROR_CHECK(esp_suit_ota_confirm());
        ESP_LOGI(TAG, "RESULT: data release confirmed");
    } else {
        ESP_ERROR_CHECK(esp_suit_ota_revert());
        ESP_LOGW(TAG, "RESULT: data release reverted");
    }
}

static void read_url(char *buf, size_t cap)
{
    if (strcmp(CONFIG_EXAMPLE_MANIFEST_URL, "FROM_STDIN") != 0) {
        strlcpy(buf, CONFIG_EXAMPLE_MANIFEST_URL, cap);
        return;
    }
    example_configure_stdin_stdout();
    printf("Enter the manifest URL (release.suit):\n");
    for (;;) {
        if (fgets(buf, cap, stdin) == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        size_t n = strlen(buf);
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ')) {
            buf[--n] = '\0';
        }
        if (n > 0) {
            return;
        }
    }
}

/*
 * The whole campaign in one loop, one resource at a time, so the application sees every boundary.
 * transport_get() pulls from the getter the current URI names, starting with the manifest, and
 * feed() parses the envelope, writes any integrated payload, and moves the context to the next
 * component with its own getter.
 */
static void run_ota(const char *url)
{
    esp_suit_ota_transport_t t = {
        .https = { .get = esp_suit_ota_https_get, .close = esp_suit_ota_https_close, .cfg = &s_https },
#if CONFIG_EXAMPLE_ENABLE_BLE
        .ble   = { .get = example_ble_payload_get, .close = example_ble_payload_close },
#endif
        .url = url,                     /* where it starts: the envelope */
    };
    uint8_t *buf = malloc(BUF_SIZE);
    if (buf == NULL) {
        ESP_LOGE(TAG, "RESULT: out of memory");
        return;
    }
    esp_suit_ota_handle_t h;
    esp_err_t err = esp_suit_ota_begin(&cfg, &t, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RESULT: begin failed: %s",
                 esp_suit_ota_err_to_name(err) ? esp_suit_ota_err_to_name(err) : esp_err_to_name(err));
        free(buf);
        return;
    }
    for (;;) {
        int n = esp_suit_ota_transport_get(&t, buf, BUF_SIZE);
        if (n < 0) {
            err = (esp_err_t)(-n);
            break;
        }
        size_t off = 0;
        do {                            /* feed stops at a boundary that redirects t */
            size_t used = 0;
            err = esp_suit_ota_feed(h, buf + off, (size_t)n - off, &used);
            off += used;
        } while (err == ESP_OK && off < (size_t)n);
        if (err != ESP_OK) {
            break;
        }
        if (n == 0 && t.done) {
            break;
        }
    }
    free(buf);

    bool restart = false;
    if (err == ESP_SUIT_OTA_READY) {
        err = esp_suit_ota_commit(h);
        if (err == ESP_OK) {
            err = esp_suit_ota_activate(h, &restart);
        }
    }
    esp_suit_ota_end(h);                /* keeps the journal, so a cut run resumes */

    if (err == ESP_OK) {
        after_activation(restart);
    } else if (err == ESP_ERR_SUIT_OTA_UP_TO_DATE || err == ESP_ERR_SUIT_OTA_NOT_MODIFIED) {
        ESP_LOGI(TAG, "RESULT: up to date (%s)", esp_suit_ota_err_to_name(err));
    } else {
        const char *name = esp_suit_ota_err_to_name(err);
        ESP_LOGE(TAG, "RESULT: failed: %s", name ? name : esp_err_to_name(err));
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_SUIT_OTA_EVENT, ESP_EVENT_ANY_ID, suit_event_handler, NULL));

    ESP_ERROR_CHECK(esp_ota_slots_init(slots, sizeof(slots) / sizeof(slots[0])));
    ESP_ERROR_CHECK(esp_suit_ota_recover());
    if (data_valid()) {
        ESP_ERROR_CHECK(esp_suit_ota_confirm());
    } else {
        ESP_LOGE(TAG, "self-test failed on the new data, rolling back");
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }

    ESP_ERROR_CHECK(example_connect());
#if CONFIG_EXAMPLE_ENABLE_BLE
    ESP_ERROR_CHECK(example_ble_payload_start());
#endif

    char url[256];
    for (;;) {
        read_url(url, sizeof(url));
        ESP_LOGI(TAG, "polling %s", url);
        run_ota(url);
    }
}
