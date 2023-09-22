/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "usb/hid_host.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HID_TEST_EVENT_HANDLE_IN_DRIVER = 0,
    HID_TEST_EVENT_HANDLE_EXTERNAL
} hid_test_event_handle_t;

// ------------------------ HID Test -------------------------------------------

void test_hid_setup(esp_event_handler_t device_callback);

void test_hid_teardown(void);

#ifdef __cplusplus
}
#endif //__cplusplus
