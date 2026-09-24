/* Wi-Fi Provisioning in SESSION_ONLY Mode Example

   This example demonstrates NETWORK_PROV_MODE_SESSION_ONLY, which starts
   the BLE transport with only prov-session and proto-ver endpoints active.
   No provisioning state machine or provisioning endpoints (prov-config,
   prov-scan, prov-ctrl) are registered by default.

   If the device is not yet provisioned, network_prov_mgr_enable_provisioning()
   is called between init and start_provisioning() to opt-in to the full
   provisioning flow.  On subsequent boots the device is already provisioned,
   so enable_provisioning() is NOT called — BLE starts for local control only
   and the provisioning endpoints are absent.

   Application endpoints (e.g. a local-control endpoint) are registered the
   same way as in any other provisioning example: call
   network_prov_mgr_endpoint_create() on NETWORK_PROV_INIT and
   network_prov_mgr_endpoint_register() on NETWORK_PROV_START.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this software is
   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
   KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include <network_provisioning/manager.h>
#include <network_provisioning/scheme_ble.h>
#include "qrcode.h"

static const char *TAG = "session_only_prov";

#define EXAMPLE_POP          "abcd1234"
#define PROV_QR_VERSION      "v1"
#define PROV_TRANSPORT_BLE   "ble"
#define QRCODE_BASE_URL      "https://espressif.github.io/esp-jumpstart/qrcode.html"

#define WIFI_CONNECTED_BIT  BIT0
static EventGroupHandle_t s_wifi_event_group;

static void wifi_prov_print_qr(const char *name, const char *pop)
{
    if (!name) {
        ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing.");
        return;
    }
    char payload[150] = {0};
    if (pop) {
        snprintf(payload, sizeof(payload),
                 "{\"ver\":\"%s\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
                 PROV_QR_VERSION, name, pop, PROV_TRANSPORT_BLE);
    } else {
        snprintf(payload, sizeof(payload),
                 "{\"ver\":\"%s\",\"name\":\"%s\",\"transport\":\"%s\"}",
                 PROV_QR_VERSION, name, PROV_TRANSPORT_BLE);
    }
#ifdef CONFIG_EXAMPLE_PROV_SHOW_QR
    ESP_LOGI(TAG, "Scan this QR code from the provisioning application for Provisioning.");
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    esp_qrcode_generate(&cfg, payload);
#endif
    ESP_LOGI(TAG, "If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s",
             QRCODE_BASE_URL, payload);
}

/* Handler for the optional custom endpoint, same as in the wifi_prov example. */
#define CUSTOM_DATA_ENDPOINT  "custom-data"

static esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf,
        ssize_t inlen, uint8_t **outbuf,
        ssize_t *outlen, void *priv_data)
{
    if (inbuf) {
        ESP_LOGI(TAG, "Received data: %.*s", (int)inlen, (char *)inbuf);
    }
    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    if (*outbuf == NULL) {
        ESP_LOGE(TAG, "System out of memory");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1;
    return ESP_OK;
}

static void prov_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == NETWORK_PROV_EVENT) {
        switch (event_id) {
        case NETWORK_PROV_INIT:
            network_prov_mgr_endpoint_create(CUSTOM_DATA_ENDPOINT);
            break;

        case NETWORK_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            network_prov_mgr_endpoint_register(CUSTOM_DATA_ENDPOINT,
                                               custom_prov_data_handler, NULL);
            break;

        case NETWORK_PROV_WIFI_CRED_RECV: {
            wifi_sta_config_t *cfg = (wifi_sta_config_t *)event_data;
            ESP_LOGI(TAG, "Received Wi-Fi credentials: SSID \"%s\"",
                     (const char *)cfg->ssid);
            break;
        }
        case NETWORK_PROV_WIFI_CRED_FAIL: {
            network_prov_wifi_sta_fail_reason_t *reason =
                (network_prov_wifi_sta_fail_reason_t *)event_data;
            ESP_LOGE(TAG, "Provisioning failed: %s",
                     (*reason == NETWORK_PROV_WIFI_STA_AUTH_ERROR) ?
                     "Wi-Fi authentication failed" : "AP not found");
            break;
        }
        case NETWORK_PROV_WIFI_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            break;

        case NETWORK_PROV_END:
            ESP_LOGI(TAG, "Provisioning ended");
            break;

        default:
            break;
        }
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&evt->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void app_main(void)
{
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,        ESP_EVENT_ANY_ID,
                    &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,          IP_EVENT_STA_GOT_IP,
                    &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID,
                    &prov_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    bool provisioned = false;
    ESP_ERROR_CHECK(network_prov_mgr_is_wifi_provisioned(&provisioned));

    uint8_t eth_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    char service_name[32];
    snprintf(service_name, sizeof(service_name), "PROV_%02X%02X%02X",
             eth_mac[3], eth_mac[4], eth_mac[5]);

    /* -----------------------------------------------------------------------
     * Initialize the provisioning manager in SESSION_ONLY mode.
     * In this mode, only prov-session and proto-ver are registered on the
     * BLE transport.  No provisioning state machine is active by default.
     *
     * When using SCHEME_BLE_EVENT_HANDLER_FREE_BLE the BT classic memory is
     * freed after provisioning; use NONE to keep full BT available.
     * --------------------------------------------------------------------- */
    network_prov_mgr_config_t config = {
        .scheme               = network_prov_scheme_ble,
        .mode                 = NETWORK_PROV_MODE_SESSION_ONLY,
        .scheme_event_handler = NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BLE,
        .app_event_handler    = NETWORK_PROV_EVENT_HANDLER_NONE,
    };
    ESP_ERROR_CHECK(network_prov_mgr_init(config));

    if (!provisioned) {
        ESP_LOGI(TAG, "Starting provisioning");

        /* Opt-in to full provisioning on this boot.  This registers the
         * provisioning GATT characteristics (prov-config, prov-scan,
         * prov-ctrl) so they are included in the BLE service table and adds
         * wifi_scan / wifi_prov to the proto-ver capabilities JSON. */
        ESP_ERROR_CHECK(network_prov_mgr_enable_provisioning());

        ESP_ERROR_CHECK(network_prov_mgr_start_provisioning(
                            NETWORK_PROV_SECURITY_1, EXAMPLE_POP, service_name, NULL));

        wifi_prov_print_qr(service_name, EXAMPLE_POP);

        network_prov_mgr_wait();
    } else {
        ESP_LOGI(TAG, "Already provisioned — starting BLE for local control only");

        /* No enable_provisioning() call here: BLE starts with only
         * prov-session + proto-ver + local-ctrl (registered via endpoint_create
         * in the NETWORK_PROV_INIT handler above). */
        ESP_ERROR_CHECK(network_prov_mgr_start_provisioning(
                            NETWORK_PROV_SECURITY_1, EXAMPLE_POP, service_name, NULL));
    }

    network_prov_mgr_deinit();

    esp_wifi_connect();
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                        false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "Connected to Wi-Fi");
}
