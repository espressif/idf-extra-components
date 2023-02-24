/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tinyusb_net.h"
#include "descriptors_control.h"
#include "usb_descriptors.h"

#define MAC_ADDR_LEN 6 // In bytes

static SemaphoreHandle_t  Net_Semphore;
static net_recv_handler_t net_recv_callback;

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

esp_err_t tinyusb_net_send(void *buffer, uint16_t len)
{
    if (!tud_ready()) {
        return ESP_FAIL;
    }
    
    if (tud_network_wait_xmit(100)) {
        /* if the network driver can accept another packet, we make it happen */
        if (tud_network_can_xmit(len)) {
            tud_network_xmit(buffer, len);
        }
    }
    return ESP_OK;
}

esp_err_t tinyusb_net_init(tinyusb_usbdev_t usb_dev, const tinyusb_net_config_t *cfg)
{
    (void) usb_dev;
    uint8_t mac_id;
    //TODO: Init the network driver
    vSemaphoreCreateBinary(Net_Semphore);

    // Convert MAC address into UTF-8
    static char mac_str[2 * MAC_ADDR_LEN + 1]; // +1 for NULL terminator
    for (unsigned i = 0; i < MAC_ADDR_LEN; i++) {
        mac_str[2 * i]     = "0123456789ABCDEF"[(cfg->mac_addr[i] >> 4) & 0x0F];
        mac_str[2 * i + 1] = "0123456789ABCDEF"[(cfg->mac_addr[i] >> 0) & 0x0F];
    }
    mac_str[2 * MAC_ADDR_LEN] = '\0';
    net_recv_callback = cfg->recv_handle;
    mac_id = tusb_get_mac_string_id();
    // Pass it to Descriptor control module
    tinyusb_set_str_descriptor(mac_str, mac_id);

    return ESP_OK;
}

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+
bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
    if (net_recv_callback) {
        net_recv_callback((void *)src, size);
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