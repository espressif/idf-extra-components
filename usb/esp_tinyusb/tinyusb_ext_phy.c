/*
 * SPDX-FileCopyrightText: 2020-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tinyusb_ext_phy.h"

static usb_phy_handle_t phy_hdl;

esp_err_t tinyusb_ext_phy_new(const tinyusb_config_t *config)
{
    // Configure USB PHY
    usb_phy_config_t phy_conf = {0};

    phy_conf.controller = USB_PHY_CTRL_OTG;
    phy_conf.otg_mode = USB_OTG_MODE_DEVICE;

    if (config->external_phy) {
        // External PHY IOs config
        usb_phy_ext_io_conf_t ext_io_conf = { 0 };
        ext_io_conf.vp_io_num = USBPHY_VP_NUM;
        ext_io_conf.vm_io_num = USBPHY_VM_NUM;
        ext_io_conf.rcv_io_num = USBPHY_RCV_NUM;
        ext_io_conf.oen_io_num = USBPHY_OEN_NUM;
        ext_io_conf.vpo_io_num = USBPHY_VPO_NUM;
        ext_io_conf.vmo_io_num = USBPHY_VMO_NUM;

        phy_conf.target = USB_PHY_TARGET_EXT;
        phy_conf.ext_io_conf = &ext_io_conf;
    } else {
        phy_conf.target = USB_PHY_TARGET_INT;
    }

    // OTG IOs config
    const usb_phy_otg_io_conf_t otg_io_conf = USB_PHY_SELF_POWERED_DEVICE(config->vbus_monitor_io);
    if (config->self_powered) {
        phy_conf.otg_io_conf = &otg_io_conf;
    }
    return usb_new_phy(&phy_conf, &phy_hdl);
}

esp_err_t tinyusb_ext_phy_delete(void)
{
    return usb_del_phy(phy_hdl);
}
