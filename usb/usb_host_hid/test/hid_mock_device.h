/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

typedef enum {
    TUSB_IFACE_COUNT_ONE = 0x00,
    TUSB_IFACE_COUNT_TWO = 0x01,
    TUSB_IFACE_COUNT_MAX
} tusb_iface_count_t;

void hid_mock_device(tusb_iface_count_t iface_count);
