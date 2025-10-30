/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_network.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "qrcode.h"
#include "lwip/ip_addr.h"
#include "network_provisioning/manager.h"
#if defined(CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_BLE)
#include "network_provisioning/scheme_ble.h"
#elif defined(CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_SOFTAP)
#include "network_provisioning/scheme_softap.h"
#endif
#include "esp_wifi_types.h"

static const char *TAG = "app_network";

/* Static variables for event group and provisioning scheme */
static EventGroupHandle_t s_event_group = NULL;
static esp_netif_t *s_sta_netif = NULL;
static uint8_t s_mac_addr[6];

#if defined(CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_SECURITY_VERSION_1)
/* Proof of Possession for secure provisioning */
#define ESP_SCHEDULE_EXAMPLE_PROV_SEC1_POP "12345678"
#elif defined(CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_SECURITY_VERSION_2)
/* Username and password for protocomm security 2 */
#define ESP_SCHEDULE_EXAMPLE_PROV_SEC2_USERNAME          "wifiprov"
#define ESP_SCHEDULE_EXAMPLE_PROV_SEC2_PWD               "abcd1234"

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

/* Event handler for network provisioning events */
static void network_prov_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == NETWORK_PROV_EVENT) {
        switch (event_id) {
        case NETWORK_PROV_START:
            ESP_LOGI(TAG, "Network provisioning started");
            break;

        case NETWORK_PROV_WIFI_CRED_RECV: {
            ESP_LOGI(TAG, "WiFi credentials received");
            wifi_config_t *wifi_config = (wifi_config_t *)event_data;
            ESP_LOGI(TAG, "SSID: %s", wifi_config->sta.ssid);
            break;
        }

        case NETWORK_PROV_WIFI_CRED_SUCCESS:
            ESP_LOGI(TAG, "Network provisioning credentials accepted");
            if (s_event_group) {
                xEventGroupSetBits(s_event_group, PROVISIONING_SUCCESS_BIT);
            }
            break;

        case NETWORK_PROV_WIFI_CRED_FAIL:
            ESP_LOGE(TAG, "Network provisioning credentials failed");
            if (s_event_group) {
                xEventGroupSetBits(s_event_group, PROVISIONING_FAILED_BIT);
            }
            break;

        case NETWORK_PROV_END:
            ESP_LOGI(TAG, "Network provisioning ended");
            break;

        default:
            ESP_LOGD(TAG, "Unhandled network provisioning event: %ld", event_id);
            break;
        }
    }
}

/* Event handler for WiFi events */
static void wifi_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi station started");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi station connected");
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            ESP_LOGW(TAG, "WiFi station disconnected");
            wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
            ESP_LOGW(TAG, "Disconnect reason: %d", event->reason);

            if (s_event_group) {
                xEventGroupSetBits(s_event_group, NETWORK_DISCONNECTED_BIT);
            }
            esp_wifi_connect();
            break;
        }

        default:
            ESP_LOGD(TAG, "Unhandled WiFi event: %ld", event_id);
            break;
        }
    }
}

/* Event handler for IP events */
static void ip_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT) {
        switch (event_id) {
        case IP_EVENT_STA_GOT_IP: {
            ESP_LOGI(TAG, "WiFi connected, got IP address");
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&event->ip_info.ip));

            if (s_event_group) {
                xEventGroupSetBits(s_event_group, NETWORK_CONNECTED_BIT);
            }
            break;
        }

        case IP_EVENT_STA_LOST_IP:
            ESP_LOGW(TAG, "WiFi disconnected, lost IP address");
            if (s_event_group) {
                xEventGroupSetBits(s_event_group, NETWORK_DISCONNECTED_BIT);
            }
            break;

        default:
            ESP_LOGD(TAG, "Unhandled IP event: %ld", event_id);
            break;
        }
    }
}

/* Initialize WiFi interfaces */
static esp_err_t wifi_init(void)
{
    esp_err_t ret;

    /* Get MAC address for service name generation */
    ret = esp_read_mac(s_mac_addr, ESP_MAC_BASE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MAC address: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Initialize TCP/IP */
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize TCP/IP: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Create default WiFi station interface */
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create WiFi station interface");
        return ESP_FAIL;
    }

    /* Initialize WiFi with default configuration */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Set WiFi storage to RAM (credentials will be managed by provisioning) */
    ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi storage: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register event handlers */
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi interfaces initialized");
    return ESP_OK;
}

/* Start WiFi station */
static esp_err_t wifi_start_sta(void)
{
    esp_err_t ret;
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode to STA: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi station: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

/* Generate QR code for provisioning data */
static void display_qr_code(const char *service_name)
{
    if (!service_name) {
        ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing.");
        return;
    }

#if defined(CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_BLE)
    const char *transport = "ble";
#elif defined(CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_SOFTAP)
    const char *transport = "softap";
#else
    ESP_LOGE(TAG, "Unknown transport; cannot generate QR code.");
    return;
#endif

    static const char *version = "v1";
    char payload[150];

#if defined(CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_SECURITY_VERSION_1)
    snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
             version, service_name, ESP_SCHEDULE_EXAMPLE_PROV_SEC1_POP, transport);
#elif defined(CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_SECURITY_VERSION_2)
    snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\",\"username\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
             version, service_name, ESP_SCHEDULE_EXAMPLE_PROV_SEC2_USERNAME, ESP_SCHEDULE_EXAMPLE_PROV_SEC2_PWD, transport);
#else
    snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\",\"transport\":\"%s\"}",
             version, service_name, transport);
#endif

    ESP_LOGI(TAG, "Scan this QR code from the ESP RainMaker phone app for Provisioning.");
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    esp_qrcode_generate(&cfg, payload);

    ESP_LOGI(TAG, "If QR code is not visible, copy paste the below URL in a browser.\nhttps://espressif.github.io/esp-jumpstart/qrcode.html?data=%s", payload);
}

/* Initialize network provisioning with event group handling */
esp_err_t app_network_init(EventGroupHandle_t event_group)
{
    esp_err_t ret;

    s_event_group = event_group;

    /* Initialize WiFi interfaces first */
    ret = wifi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi");
        return ret;
    }

    /* Initialize network provisioning manager */
    network_prov_mgr_config_t config = {
#if defined(CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_BLE)
        .scheme = network_prov_scheme_ble,
        .scheme_event_handler = NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BLE,
#elif defined(CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_SOFTAP)
        .scheme = network_prov_scheme_softap,
        .scheme_event_handler = NETWORK_PROV_EVENT_HANDLER_NONE,
#endif
    };

    ret = network_prov_mgr_init(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize network provisioning manager: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register event handlers */
    ret = esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID, &network_prov_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register network provisioning event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Network provisioning initialized successfully");
    return ESP_OK;
}

/* Start network provisioning and wait for connection */
esp_err_t app_network_start(EventGroupHandle_t event_group, uint32_t timeout_ms)
{
    esp_err_t ret;
    bool provisioned = false;

    /* Check if device is already provisioned */
    ret = network_prov_mgr_is_wifi_provisioned(&provisioned);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to check provisioning status: %s", esp_err_to_name(ret));
        return ret;
    }

    if (!provisioned) {
        ESP_LOGI(TAG, "Device not provisioned, starting provisioning...");

        /* Generate service name */
        char service_name[32];
        snprintf(service_name, sizeof(service_name), "ESP-Schedule-%02x%02x%02x",
                 s_mac_addr[3], s_mac_addr[4], s_mac_addr[5]);

        /* Display QR code for provisioning */
        display_qr_code(service_name);
#if defined(CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_SOFTAP)
        /* For SoftAP, create WiFi AP interface */
        esp_netif_create_default_wifi_ap();
#endif

#if defined(CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_SECURITY_VERSION_0)
        ret = network_prov_mgr_start_provisioning(NETWORK_PROV_SECURITY_0, NULL, service_name, NULL);
#endif
#if defined(CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_SECURITY_VERSION_1)
        ret = network_prov_mgr_start_provisioning(NETWORK_PROV_SECURITY_1, ESP_SCHEDULE_EXAMPLE_PROV_SEC1_POP, service_name, NULL);
#elif defined(CONFIG_ESP_SCHEDULE_EXAMPLE_PROV_SECURITY_VERSION_2)
        const network_prov_security2_params_t sec2_params = {
            .salt = sec2_salt,
            .salt_len = sizeof(sec2_salt),
            .verifier = sec2_verifier,
            .verifier_len = sizeof(sec2_verifier),
        };
        ret = network_prov_mgr_start_provisioning(NETWORK_PROV_SECURITY_2, &sec2_params, service_name, NULL);
#endif
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start network provisioning: %s", esp_err_to_name(ret));
            return ret;
        }
    } else {
        ESP_LOGI(TAG, "Device already provisioned, starting Wi-Fi STA");

        /* Start WiFi station directly */
        ret = wifi_start_sta();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start WiFi station: %s", esp_err_to_name(ret));
            return ret;
        }

        /* Post provisioning end event since we're already provisioned */
        esp_event_post(NETWORK_PROV_EVENT, NETWORK_PROV_END, NULL, 0, portMAX_DELAY);
    }

    /* Wait for network connection */
    EventBits_t bits = xEventGroupWaitBits(event_group,
                                           NETWORK_CONNECTED_BIT,
                                           pdTRUE,  // Clear bits on exit
                                           pdFALSE, // Wait for connection only
                                           pdMS_TO_TICKS(timeout_ms));

    if (bits & NETWORK_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Network connected successfully");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Network connection timed out");
        return ESP_ERR_TIMEOUT;
    }
}

/* Start time synchronization */
void app_network_start_time_sync(EventGroupHandle_t event_group)
{
    ESP_LOGI(TAG, "Starting SNTP time synchronization...");

    /* Configure SNTP */
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_setservername(2, "time.cloudflare.com");

    /* Start SNTP */
    esp_sntp_init();

    /* Set a flag to indicate time sync has started */
    if (event_group) {
        xEventGroupSetBits(event_group, TIME_SYNC_SUCCESS_BIT);
    }
}

/* Wait for time synchronization to complete */
esp_err_t app_network_wait_for_time_sync(EventGroupHandle_t event_group, uint32_t timeout_ms)
{
    time_t now = 0;
    int retry = 0;
    const int retry_count = timeout_ms / 2000;  // Check every 2 seconds
    const time_t time_threshold = 1609459200;  // January 1, 2021 - reasonable baseline

    ESP_LOGI(TAG, "Waiting for time synchronization...");

    while (retry < retry_count) {
        time(&now);

        if (now >= time_threshold && sntp_get_sync_status() != SNTP_SYNC_STATUS_RESET) {
            ESP_LOGI(TAG, "Time synchronized successfully! Current time: %s", ctime(&now));
            if (event_group) {
                xEventGroupSetBits(event_group, TIME_SYNC_SUCCESS_BIT);
            }
            return ESP_OK;
        }

        ESP_LOGD(TAG, "Time sync attempt %d/%d, time: %ld, status: %d",
                 retry + 1, retry_count, (long)now, sntp_get_sync_status());

        vTaskDelay(2000 / portTICK_PERIOD_MS);
        retry++;
    }

    ESP_LOGW(TAG, "Time synchronization may have failed or time is before threshold");
    if (event_group) {
        xEventGroupSetBits(event_group, TIME_SYNC_FAILED_BIT);
    }
    return ESP_ERR_TIMEOUT;
}