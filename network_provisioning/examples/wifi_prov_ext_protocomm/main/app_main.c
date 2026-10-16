/* Wi-Fi Provisioning in SESSION_ONLY Mode Example

   Demonstrates NETWORK_PROV_MODE_SESSION_ONLY: the BLE transport and the
   secure protocomm session stay up after provisioning, and on boots where
   the device is already provisioned, so that the application endpoint
   "custom-data" keeps being served. See README.md for details.

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

#if CONFIG_EXAMPLE_PROV_SECURITY_VERSION_2
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
#define EXAMPLE_PROV_SEC2_USERNAME          "wifiprov"
#define EXAMPLE_PROV_SEC2_PWD               "abcd1234"

/* This salt,verifier has been generated for username = "wifiprov" and password = "abcd1234"
 * IMPORTANT NOTE: For production cases, this must be unique to every device
 * and should come from device manufacturing partition.*/
static const char sec2_salt[] = {
    0x03, 0x6e, 0xe0, 0xc7, 0xbc, 0xb9, 0xed, 0xa8, 0x4c, 0x9e, 0xac, 0x97, 0xd9, 0x3d, 0xec, 0xf4
};

static const char sec2_verifier[] = {
    0x7c, 0x7c, 0x85, 0x47, 0x65, 0x08, 0x94, 0x6d, 0xd6, 0x36, 0xaf, 0x37, 0xd7, 0xe8, 0x91, 0x43,
    0x78, 0xcf, 0xfd, 0x61, 0x6c, 0x59, 0xd2, 0xf8, 0x39, 0x08, 0x12, 0x72, 0x38, 0xde, 0x9e, 0x24,
    0xa4, 0x70, 0x26, 0x1c, 0xdf, 0xa9, 0x03, 0xc2, 0xb2, 0x70, 0xe7, 0xb1, 0x32, 0x24, 0xda, 0x11,
    0x1d, 0x97, 0x18, 0xdc, 0x60, 0x72, 0x08, 0xcc, 0x9a, 0xc9, 0x0c, 0x48, 0x27, 0xe2, 0xae, 0x89,
    0xaa, 0x16, 0x25, 0xb8, 0x04, 0xd2, 0x1a, 0x9b, 0x3a, 0x8f, 0x37, 0xf6, 0xe4, 0x3a, 0x71, 0x2e,
    0xe1, 0x27, 0x86, 0x6e, 0xad, 0xce, 0x28, 0xff, 0x54, 0x46, 0x60, 0x1f, 0xb9, 0x96, 0x87, 0xdc,
    0x57, 0x40, 0xa7, 0xd4, 0x6c, 0xc9, 0x77, 0x54, 0xdc, 0x16, 0x82, 0xf0, 0xed, 0x35, 0x6a, 0xc4,
    0x70, 0xad, 0x3d, 0x90, 0xb5, 0x81, 0x94, 0x70, 0xd7, 0xbc, 0x65, 0xb2, 0xd5, 0x18, 0xe0, 0x2e,
    0xc3, 0xa5, 0xf9, 0x68, 0xdd, 0x64, 0x7b, 0xb8, 0xb7, 0x3c, 0x9c, 0xfc, 0x00, 0xd8, 0x71, 0x7e,
    0xb7, 0x9a, 0x7c, 0xb1, 0xb7, 0xc2, 0xc3, 0x18, 0x34, 0x29, 0x32, 0x43, 0x3e, 0x00, 0x99, 0xe9,
    0x82, 0x94, 0xe3, 0xd8, 0x2a, 0xb0, 0x96, 0x29, 0xb7, 0xdf, 0x0e, 0x5f, 0x08, 0x33, 0x40, 0x76,
    0x52, 0x91, 0x32, 0x00, 0x9f, 0x97, 0x2c, 0x89, 0x6c, 0x39, 0x1e, 0xc8, 0x28, 0x05, 0x44, 0x17,
    0x3f, 0x68, 0x02, 0x8a, 0x9f, 0x44, 0x61, 0xd1, 0xf5, 0xa1, 0x7e, 0x5a, 0x70, 0xd2, 0xc7, 0x23,
    0x81, 0xcb, 0x38, 0x68, 0xe4, 0x2c, 0x20, 0xbc, 0x40, 0x57, 0x76, 0x17, 0xbd, 0x08, 0xb8, 0x96,
    0xbc, 0x26, 0xeb, 0x32, 0x46, 0x69, 0x35, 0x05, 0x8c, 0x15, 0x70, 0xd9, 0x1b, 0xe9, 0xbe, 0xcc,
    0xa9, 0x38, 0xa6, 0x67, 0xf0, 0xad, 0x50, 0x13, 0x19, 0x72, 0x64, 0xbf, 0x52, 0xc2, 0x34, 0xe2,
    0x1b, 0x11, 0x79, 0x74, 0x72, 0xbd, 0x34, 0x5b, 0xb1, 0xe2, 0xfd, 0x66, 0x73, 0xfe, 0x71, 0x64,
    0x74, 0xd0, 0x4e, 0xbc, 0x51, 0x24, 0x19, 0x40, 0x87, 0x0e, 0x92, 0x40, 0xe6, 0x21, 0xe7, 0x2d,
    0x4e, 0x37, 0x76, 0x2f, 0x2e, 0xe2, 0x68, 0xc7, 0x89, 0xe8, 0x32, 0x13, 0x42, 0x06, 0x84, 0x84,
    0x53, 0x4a, 0xb3, 0x0c, 0x1b, 0x4c, 0x8d, 0x1c, 0x51, 0x97, 0x19, 0xab, 0xae, 0x77, 0xff, 0xdb,
    0xec, 0xf0, 0x10, 0x95, 0x34, 0x33, 0x6b, 0xcb, 0x3e, 0x84, 0x0f, 0xb9, 0xd8, 0x5f, 0xb8, 0xa0,
    0xb8, 0x55, 0x53, 0x3e, 0x70, 0xf7, 0x18, 0xf5, 0xce, 0x7b, 0x4e, 0xbf, 0x27, 0xce, 0xce, 0xa8,
    0xb3, 0xbe, 0x40, 0xc5, 0xc5, 0x32, 0x29, 0x3e, 0x71, 0x64, 0x9e, 0xde, 0x8c, 0xf6, 0x75, 0xa1,
    0xe6, 0xf6, 0x53, 0xc8, 0x31, 0xa8, 0x78, 0xde, 0x50, 0x40, 0xf7, 0x62, 0xde, 0x36, 0xb2, 0xba
};
#endif

static esp_err_t example_get_sec2_salt(const char **salt, uint16_t *salt_len)
{
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
    ESP_LOGI(TAG, "Development mode: using hard coded salt");
    *salt = sec2_salt;
    *salt_len = sizeof(sec2_salt);
    return ESP_OK;
#elif CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
    ESP_LOGE(TAG, "Not implemented!");
    return ESP_FAIL;
#endif
}

static esp_err_t example_get_sec2_verifier(const char **verifier, uint16_t *verifier_len)
{
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
    ESP_LOGI(TAG, "Development mode: using hard coded verifier");
    *verifier = sec2_verifier;
    *verifier_len = sizeof(sec2_verifier);
    return ESP_OK;
#elif CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
    /* This code needs to be updated with appropriate implementation to provide verifier */
    ESP_LOGE(TAG, "Not implemented!");
    return ESP_FAIL;
#endif
}
#endif

#define PROV_QR_VERSION      "v1"
#define PROV_TRANSPORT_BLE   "ble"
#define QRCODE_BASE_URL      "https://espressif.github.io/esp-jumpstart/qrcode.html"

#define WIFI_CONNECTED_BIT  BIT0
static EventGroupHandle_t s_wifi_event_group;

static void wifi_prov_print_qr(const char *name, const char *username, const char *pop)
{
    if (!name) {
        ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing.");
        return;
    }
    char payload[150] = {0};
    if (pop) {
#if CONFIG_EXAMPLE_PROV_SECURITY_VERSION_1
        snprintf(payload, sizeof(payload),
                 "{\"ver\":\"%s\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
                 PROV_QR_VERSION, name, pop, PROV_TRANSPORT_BLE);
#elif CONFIG_EXAMPLE_PROV_SECURITY_VERSION_2
        snprintf(payload, sizeof(payload),
                 "{\"ver\":\"%s\",\"name\":\"%s\",\"username\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
                 PROV_QR_VERSION, name, username, pop, PROV_TRANSPORT_BLE);
#endif
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
        case NETWORK_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            network_prov_mgr_endpoint_register(CUSTOM_DATA_ENDPOINT,
                                               custom_prov_data_handler, NULL);
            break;

        case NETWORK_PROV_WIFI_CRED_RECV: {
            wifi_sta_config_t *cfg = (wifi_sta_config_t *)event_data;
            ESP_LOGI(TAG, "Received Wi-Fi credentials: SSID \"%.*s\"",
                     (int)sizeof(cfg->ssid), (const char *)cfg->ssid);
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

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID,
                                               &prov_event_handler, NULL));

    bool provisioned = false;
#ifdef CONFIG_EXAMPLE_RESET_PROVISIONED
    /* Erase the stored Wi-Fi credentials so that provisioning runs on every boot */
    ESP_ERROR_CHECK(network_prov_mgr_reset_wifi_provisioning());
#else
    ESP_ERROR_CHECK(network_prov_mgr_is_wifi_provisioned(&provisioned));
#endif

    /* In SESSION_ONLY mode the manager only touches Wi-Fi while provisioning
     * is enabled, so start the station ourselves */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

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
     * The FREE_BLE scheme handler releases the BLE memory on NETWORK_PROV_DEINIT.
     * This example never calls network_prov_mgr_deinit(), so it only matters
     * for applications that eventually do.
     * --------------------------------------------------------------------- */
    network_prov_mgr_config_t config = {
        .scheme               = network_prov_scheme_ble,
        .mode                 = NETWORK_PROV_MODE_SESSION_ONLY,
        .scheme_event_handler = NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BLE,
        .app_event_handler    = NETWORK_PROV_EVENT_HANDLER_NONE,
    };
    ESP_ERROR_CHECK(network_prov_mgr_init(config));

    /* Create the application endpoint before the transport is started. In
     * SESSION_ONLY mode the transport keeps running after provisioning, so
     * this is the only opportunity: endpoints cannot be created later. */
    ESP_ERROR_CHECK(network_prov_mgr_endpoint_create(CUSTOM_DATA_ENDPOINT));

#ifdef CONFIG_EXAMPLE_PROV_SECURITY_VERSION_1
    /* Security 1: X25519 key exchange + proof of possession, AES-CTR.
     * The manager keeps its own copy of the PoP. */
    network_prov_security_t security = NETWORK_PROV_SECURITY_1;
    const char *username = NULL;
    const char *pop = "abcd1234";
    network_prov_security1_params_t *sec_params = pop;
#elif CONFIG_EXAMPLE_PROV_SECURITY_VERSION_2
    /* Security 2: SRP6a authentication + AES-GCM. The username must match
     * the one used to generate the salt and verifier. */
    network_prov_security_t security = NETWORK_PROV_SECURITY_2;
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
    /* The password is only kept here to print it in the QR code. In
     * production it must not be stored on the device. */
    const char *username = EXAMPLE_PROV_SEC2_USERNAME;
    const char *pop = EXAMPLE_PROV_SEC2_PWD;
#elif CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
    /* Username and password are provided to the user by other means,
     * e.g. a QR code sticker. */
    const char *username = NULL;
    const char *pop = NULL;
#endif
    /* In SESSION_ONLY mode the security handler keeps serving new sessions
     * after provisioning stops, so these parameters must stay valid until
     * network_prov_mgr_deinit(), not just until NETWORK_PROV_END. Hence they
     * do not live on the app_main() stack. */
    static network_prov_security2_params_t sec2_params = {};
    ESP_ERROR_CHECK(example_get_sec2_salt(&sec2_params.salt, &sec2_params.salt_len));
    ESP_ERROR_CHECK(example_get_sec2_verifier(&sec2_params.verifier, &sec2_params.verifier_len));
    network_prov_security2_params_t *sec_params = &sec2_params;
#endif

    if (!provisioned) {
        ESP_LOGI(TAG, "Starting provisioning");

        /* Opt-in to full provisioning on this boot.  This registers the
         * provisioning GATT characteristics (prov-config, prov-scan,
         * prov-ctrl) so they are included in the BLE service table and adds
         * wifi_scan / wifi_prov to the proto-ver capabilities JSON. */
        ESP_ERROR_CHECK(network_prov_mgr_enable_provisioning());

        ESP_ERROR_CHECK(network_prov_mgr_start_provisioning(security, sec_params,
                                                            service_name, NULL));

        wifi_prov_print_qr(service_name, username, pop);

        /* Returns once provisioning has stopped. The manager has connected to
         * the received Wi-Fi network by then. In SESSION_ONLY mode the BLE
         * transport, the secure session and the custom-data endpoint stay up;
         * only the provisioning endpoints are removed. */
        network_prov_mgr_wait();
    } else {
        ESP_LOGI(TAG, "Already provisioned, starting BLE for the custom-data endpoint only");

        /* No enable_provisioning() call here: BLE starts with only
         * prov-session + proto-ver + custom-data. The manager does not touch
         * Wi-Fi in this case, so connect to the stored network ourselves. */
        ESP_ERROR_CHECK(network_prov_mgr_start_provisioning(security, sec_params,
                                                            service_name, NULL));
        esp_wifi_connect();
    }

    /* network_prov_mgr_deinit() is NOT called: it is the only call that tears
     * the BLE transport down, and the point of SESSION_ONLY mode is to keep
     * serving the custom-data endpoint over the same session. */
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                        false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "Connected to Wi-Fi, custom-data endpoint stays reachable over BLE");
}
