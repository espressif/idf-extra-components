/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdint.h>
#include "sdkconfig.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "esp_idf_version.h"
#include "hid_mock_device.h"

static tusb_iface_count_t tusb_iface_count = 0;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
/************* TinyUSB descriptors ****************/
#define TUSB_DESC_TOTAL_LEN      (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

/**
 * @brief HID report descriptor
 *
 * In this example we implement Keyboard + Mouse HID device,
 * so we must define both report descriptors
 */
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD) ),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_ITF_PROTOCOL_MOUSE) )
};

const uint8_t hid_keyboard_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD) )
};

const uint8_t hid_mouse_report_descriptor[] = {
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_ITF_PROTOCOL_MOUSE) )
};

/**
 * @brief String descriptor
 */
const char *hid_string_descriptor[5] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
    "TinyUSB",             // 1: Manufacturer
    "TinyUSB Device",      // 2: Product
    "123456",              // 3: Serials, should use chip ID
    "Example HID interface",  // 4: HID
};

/**
 * @brief Configuration descriptor
 *
 * This is a simple configuration descriptor that defines 1 configuration and 1 HID interface
 */
static const uint8_t hid_configuration_descriptor_one_iface[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, CFG_TUD_HID, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
    TUD_HID_DESCRIPTOR(1, 4, false, sizeof(hid_report_descriptor), 0x82, 16, 10),
};

static const uint8_t hid_configuration_descriptor_two_ifaces[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, CFG_TUD_HID, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 4, HID_ITF_PROTOCOL_KEYBOARD, sizeof(hid_keyboard_report_descriptor), 0x81, 16, 10),
    TUD_HID_DESCRIPTOR(1, 4, HID_ITF_PROTOCOL_MOUSE, sizeof(hid_mouse_report_descriptor), 0x82, 16, 10),
};

static const uint8_t *hid_configuration_descriptor_list[TUSB_IFACE_COUNT_MAX] = {
    hid_configuration_descriptor_one_iface,
    hid_configuration_descriptor_two_ifaces
};
#endif // // esp idf >= v5.0.0
/********* TinyUSB HID callbacks ***************/

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
// Invoked when received GET HID REPORT DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    switch (tusb_iface_count) {
    case TUSB_IFACE_COUNT_ONE:
        return hid_report_descriptor;

    case TUSB_IFACE_COUNT_TWO:
        return (!!instance) ? hid_mouse_report_descriptor : hid_keyboard_report_descriptor;

    default:
        break;
    }
    return NULL;
}
#endif // esp idf >= v5.0.0

#if ((ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)) && (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)))
#define HID_ITF_PROTOCOL_KEYBOARD HID_PROTOCOL_KEYBOARD
#define HID_ITF_PROTOCOL_MOUSE    HID_PROTOCOL_MOUSE
#endif // 4.4.0 <= esp idf < v5.0.0

/**
 * @brief Get Keyboard report
 *
 * Fill buffer with test Keyboard report data
 *
 * @param[in] buffer   Pointer to a buffer for filling
 * @return uint16_t    Length of copied data to buffer
 */
static inline uint16_t get_keyboard_report(uint8_t *buffer)
{
    hid_keyboard_report_t kb_report = {
        0,                              // Keyboard modifier
        0,                              // Reserved
        { HID_KEY_M, HID_KEY_N, HID_KEY_O, HID_KEY_P, HID_KEY_Q, HID_KEY_R }
    };
    memcpy(buffer, &kb_report, sizeof(kb_report));
    return sizeof(kb_report);
}

/**
 * @brief Get Mouse report
 *
 * Fill buffer with test Mouse report data
 *
 * @param[in] buffer   Pointer to a buffer for filling
 * @return uint16_t    Length of copied data to buffer
 */
static inline uint16_t get_mouse_report(uint8_t *buffer)
{
    hid_mouse_report_t mouse_report = {
        MOUSE_BUTTON_LEFT | MOUSE_BUTTON_RIGHT, // buttons
        -1,                                     // x
        127,                                    // y
        0,                                      // wheel
        0                                       // pan
    };
    memcpy(buffer, &mouse_report, sizeof(mouse_report));
    return sizeof(mouse_report);
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    switch (report_id) {
    case HID_ITF_PROTOCOL_KEYBOARD:
        return get_keyboard_report(buffer);

    case HID_ITF_PROTOCOL_MOUSE:
        return get_mouse_report(buffer);

    default:
        printf("HID mock device, Unhandled ReportID %d\n", report_id);
        break;
    }
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{

}

/**
 * @brief HID Mock device start
 *
 * @param[in] iface_count   Interface count, when TUSB_IFACE_COUNT_ONE then there is two Interfaces, but equal (Protocol=None).
 * when TUSB_IFACE_COUNT_TWO then HID device mocked with two independed Interfaces (Protocol=BootKeyboard, Protocol=BootMouse).
 */
void hid_mock_device(tusb_iface_count_t iface_count)
{
    if (iface_count > TUSB_IFACE_COUNT_MAX) {
        printf("UHID mock device, wrong iface_count paramteter (%d)\n",
               iface_count);
        return;
    }

    // Global Interfaces count value
    tusb_iface_count = iface_count;

    const tinyusb_config_t tusb_cfg = {
        .external_phy = false,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .device_descriptor = NULL,
        .string_descriptor = hid_string_descriptor,
        .string_descriptor_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]),
        .configuration_descriptor = hid_configuration_descriptor_list[tusb_iface_count],
#endif // esp idf >= v5.0.0
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    printf("HID mock device with %s has been started\n",
           (TUSB_IFACE_COUNT_ONE == tusb_iface_count)
           ? "1xInterface (Protocol=None)"
           : "2xInterfaces (Protocol=BootKeyboard, Protocol=BootMouse)");
}
