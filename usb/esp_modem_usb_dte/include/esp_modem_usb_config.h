/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief USB configuration structure
 * @see USB host CDC-ACM driver documentation for details about interfaces settings
 */
struct esp_modem_usb_term_config {
    uint16_t vid;                /*!< Vendor ID of the USB device */
    uint16_t pid;                /*!< Product ID of the USB device */
    int interface_idx;           /*!< USB Interface index that will be used for primary terminal: AT commands */
    int secondary_interface_idx; /*!< USB Interface index that will be used for secondary terminal: data. Set to -1 for modems with 1 AT port. */
    uint32_t timeout_ms;         /*!< Time for a USB modem to connect to USB host. 0 means wait forever. */
    int xCoreID;                 /*!< Core affinity of created tasks: CDC-ACM driver task and optional USB Host task */
    bool cdc_compliant;          /*!< Treat the USB device as CDC-compliant. Read CDC-ACM driver documentation for more details */
    bool install_usb_host;       /*!< Flag whether USB Host driver should be installed */
};

/**
 * @brief ESP Mode USB DTE Default Configuration
 *
 * @param[in] _usb_config esp_modem_usb_term_config configuration structure. Can be obtained by ESP_MODEM_DEFAULT_USB_CONFIG
 *
 */
#define ESP_MODEM_DTE_DEFAULT_USB_CONFIG(_usb_config) \
    {                                                 \
        .dte_buffer_size = 512,                       \
        .task_stack_size = 4096,                      \
        .task_priority = 5,                           \
        .extension_config = &_usb_config              \
    }

/**
 * @brief ESP Modem USB Default Configuration
 *
 * @param[in] _vid   USB Vendor ID
 * @param[in] _pid   USB Product ID
 * @param[in] _intf  USB interface number of primary AT port
 * @param[in] _intf2 USB interface number of secondary AT port
 * @see USB host CDC-ACM driver documentation for details about interfaces settings
 */
#define ESP_MODEM_DEFAULT_USB_CONFIG_DUAL(_vid, _pid, _intf, _intf2) \
    {                                                                \
        .vid = _vid,                                                 \
        .pid = _pid,                                                 \
        .interface_idx = _intf,                                      \
        .secondary_interface_idx = _intf2,                           \
        .timeout_ms = 0,                                             \
        .xCoreID = 0,                                                \
        .cdc_compliant = false,                                      \
        .install_usb_host = true                                     \
    }
#define ESP_MODEM_DEFAULT_USB_CONFIG(_vid, _pid, _intf) ESP_MODEM_DEFAULT_USB_CONFIG_DUAL(_vid, _pid, _intf, -1)

/**
 * @brief Default configuration of Quactel BG96 modem
 */
#define ESP_MODEM_BG96_USB_CONFIG()    ESP_MODEM_DEFAULT_USB_CONFIG(0x2C7C, 0x0296, 2)

/**
 * @brief Default configuration of SimCom SIM7600E modem
 */
#define ESP_MODEM_SIM7600_USB_CONFIG() ESP_MODEM_DEFAULT_USB_CONFIG(0x1E0E, 0x9001, 3)

/**
 * @brief Default configuration of SimCom A7670E modem
 */
#define ESP_MODEM_A7670_USB_CONFIG()   ESP_MODEM_DEFAULT_USB_CONFIG_DUAL(0x1E0E, 0x9011, 4, 5)
