/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_private/wifi.h"

#include "tinyusb_net.h"
#include "descriptors_control.h"
#include "usb_descriptors.h"

#define MAC_ADDR_LEN 6 // In bytes

static EventGroupHandle_t wifi_event_group;
static SemaphoreHandle_t  Net_Semphore;
const int CONNECTED_BIT = BIT0;
const int DISCONNECTED_BIT = BIT1;
static bool s_wifi_is_connected = false;

static const char *TAG = "usb_ncm";

bool tud_network_wait_xmit(uint32_t ms)
{
    if (xSemaphoreTake(Net_Semphore, ms/portTICK_PERIOD_MS) == pdTRUE) {
        xSemaphoreGive(Net_Semphore);
        return true;
    }
    return false;
}

void tusb_net_init(void)
{
}

static esp_err_t pkt_wifi2usb(void *buffer, uint16_t len, void *eb)
{
    if (!tud_ready()) {
        esp_wifi_internal_free_rx_buffer(eb);
        return ESP_FAIL;
    }
    
    if (tud_network_wait_xmit(100)) {
        /* if the network driver can accept another packet, we make it happen */
        if (tud_network_can_xmit(len)) {
            tud_network_xmit(buffer, len);
        }
    }

    esp_wifi_internal_free_rx_buffer(eb);
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi STA disconnected");
        s_wifi_is_connected = false;
        esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, NULL);

        if (tud_ready()) {
            ESP_LOGI(TAG, "sta disconnect, reconnect...");
            esp_wifi_connect();
        } else {
            ESP_LOGI(TAG, "sta disconnect");
        }
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        xEventGroupSetBits(wifi_event_group, DISCONNECTED_BIT);
        ESP_LOGI(TAG, "DISCONNECTED_BIT\r\n");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi STA connected");
        esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, pkt_wifi2usb);
        s_wifi_is_connected = true;
        xEventGroupClearBits(wifi_event_group, DISCONNECTED_BIT);
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        ESP_LOGI(TAG, "CONNECTED_BIT\r\n");
    }

}

/* Initialize Wi-Fi as sta and set scan method */
static void initialise_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_start() );
}

esp_err_t tinyusb_net_init(tinyusb_usbdev_t usb_dev)
{
    (void) usb_dev;
    uint8_t mac_addr[MAC_ADDR_LEN];
    uint8_t mac_id;
    //TODO: Init the network driver
    vSemaphoreCreateBinary(Net_Semphore);
    initialise_wifi();
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac_addr);
    printf("Mac: %x. %x, %x\n", mac_addr[0], mac_addr[1], mac_addr[2]);
    // Convert MAC address into UTF-8
    static char mac_str[2 * MAC_ADDR_LEN + 1]; // +1 for NULL terminator
    for (unsigned i = 0; i < MAC_ADDR_LEN; i++) {
        mac_str[2 * i]     = "0123456789ABCDEF"[(mac_addr[i] >> 4) & 0x0F];
        mac_str[2 * i + 1] = "0123456789ABCDEF"[(mac_addr[i] >> 0) & 0x0F];
    }
    mac_str[2 * MAC_ADDR_LEN] = '\0';
    mac_id = tusb_get_mac_string_id();
    ESP_LOGI(TAG, "Mac id: %d\r\n", mac_id);
    // Pass it to Descriptor control module
    tinyusb_set_str_descriptor(mac_str, mac_id);

    return ESP_OK;
}

esp_err_t tinyusb_net_connet_wifi(const tinyusb_wifi_config_t *cfg)
{
    if (cfg->ssid == NULL) {
        ESP_LOGI(TAG, "SSID cannot set NULL\n");
        return ESP_FAIL;
    }
    EventBits_t status = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);
    wifi_config_t wifi_config = {};
    memcpy(wifi_config.sta.ssid, cfg->ssid, strlen(cfg->ssid));
    if (cfg->pwd) {
        memcpy(wifi_config.sta.password, cfg->pwd, strlen(cfg->pwd));
    }
    
    if (status & CONNECTED_BIT) {
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        ESP_ERROR_CHECK( esp_wifi_disconnect() );

        xEventGroupWaitBits(wifi_event_group, DISCONNECTED_BIT, 0, 1, 1000/portTICK_PERIOD_MS);
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    esp_wifi_connect();

    status = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 5000/portTICK_PERIOD_MS);

    if (status & CONNECTED_BIT) {
        ESP_LOGI(TAG, "connect success\n");
        return ESP_OK;
    } else {
        ESP_LOGI(TAG, "connect fail\n");
        return ESP_ERR_TIMEOUT;
    }
}

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+

bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
    if (s_wifi_is_connected) {
        esp_wifi_internal_tx(ESP_IF_WIFI_STA, (void *)src, size);
    }
    tud_network_recv_renew();
    return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    uint16_t len = arg;

    /* traverse the "pbuf chain"; see ./lwip/src/core/pbuf.c for more info */
    memcpy(dst, ref, len);
    return len;
}

void tud_network_init_cb(void)
{
    /* TODO */
}

void tud_network_idle_status_change_cb(bool enable)
{
    if (enable == true) {
        xSemaphoreGive(Net_Semphore);
    } else {
        xSemaphoreTake(Net_Semphore, 0);
    }
}