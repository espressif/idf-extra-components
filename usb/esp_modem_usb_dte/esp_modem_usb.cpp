/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_modem_config.h"
#include "esp_modem_usb_config.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_exception.hpp"
#include "exception_stub.hpp"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "sdkconfig.h"
#include "usb_terminal.hpp"

static const char *TAG = "usb_terminal";

/**
 * @brief USB Host task
 *
 * This task is created only if install_usb_host is set to true in DTE configuration.
 * In case you don't want to install USB Host driver here, you must install it before creating UsbTerminal object.
 *
 * This implementation of USB Host Lib handling never returns, which means that the USB Host Lib will keep running
 * even after all USB devices are disconnected. That allows repeated device reconnections.
 *
 * If you want/need to handle lifetime of USB Host Lib, you can set install_usb_host to false and manage it yourself.
 *
 * @param arg Unused
 */
static void usb_host_task(void *arg)
{
    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGD(TAG, "No more clients: clean up\n");
            usb_host_device_free_all();
        }
    }
}

namespace esp_modem {
class UsbTerminal : public Terminal, private CdcAcmDevice {
public:
    explicit UsbTerminal(const esp_modem_dte_config *config, int term_idx)
    {
        const struct esp_modem_usb_term_config *usb_config = (struct esp_modem_usb_term_config *)(config->extension_config);

        // Install USB Host driver (if not already installed)
        if (usb_config->install_usb_host && !usb_host_lib_task) {
            const usb_host_config_t host_config = {
                .skip_phy_setup = false,
                .intr_flags = ESP_INTR_FLAG_LEVEL1,
            };
            ESP_MODEM_THROW_IF_ERROR(usb_host_install(&host_config), "USB Host install failed");
            ESP_LOGD(TAG, "USB Host installed");
            ESP_MODEM_THROW_IF_FALSE(
                pdTRUE == xTaskCreatePinnedToCore(usb_host_task, "usb_host", 4096, NULL, config->task_priority + 1, &usb_host_lib_task, usb_config->xCoreID),
                "USB host task failed");
        }

        // Install CDC-ACM driver
        const cdc_acm_host_driver_config_t esp_modem_cdc_acm_driver_config = {
            .driver_task_stack_size = config->task_stack_size,
            .driver_task_priority = config->task_priority,
            .xCoreID = (BaseType_t)usb_config->xCoreID,
            .new_dev_cb = NULL, // We don't forward this information to user. User can poll USB Host Lib.
        };

        // Silently continue on error: CDC-ACM driver might be already installed
        cdc_acm_host_install(&esp_modem_cdc_acm_driver_config);

        // Open CDC-ACM device
        const cdc_acm_host_device_config_t esp_modem_cdc_acm_device_config = {
            .connection_timeout_ms = usb_config->timeout_ms,
            .out_buffer_size = config->dte_buffer_size,
            .in_buffer_size = config->dte_buffer_size,
            .event_cb = handle_notif,
            .data_cb = handle_rx,
            .user_arg = this
        };

        // Determine Terminal interface index
        const uint8_t intf_idx = term_idx == 0 ? usb_config->interface_idx : usb_config->secondary_interface_idx;

        if (usb_config->cdc_compliant) {
            ESP_MODEM_THROW_IF_ERROR(
                this->CdcAcmDevice::open(usb_config->vid, usb_config->pid, intf_idx, &esp_modem_cdc_acm_device_config),
                "USB Device open failed");
        } else {
            ESP_MODEM_THROW_IF_ERROR(
                this->CdcAcmDevice::open_vendor_specific(usb_config->vid, usb_config->pid, intf_idx, &esp_modem_cdc_acm_device_config),
                "USB Device open failed");
        }
    };

    ~UsbTerminal()
    {
        this->CdcAcmDevice::close();
    };

    void start() override
    {
        return;
    }

    void stop() override
    {
        return;
    }

    int write(uint8_t *data, size_t len) override
    {
        ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_DEBUG);
        if (this->CdcAcmDevice::tx_blocking(data, len) != ESP_OK) {
            return -1;
        }
        return len;
    }

    int read(uint8_t *data, size_t len) override
    {
        // This function should never be called. UsbTerminal provides data through Terminal::on_read callback
        ESP_LOGW(TAG, "Unexpected call to UsbTerminal::read function");
        return -1;
    }

private:
    UsbTerminal() = delete;
    UsbTerminal(const UsbTerminal &copy) = delete;
    UsbTerminal &operator=(const UsbTerminal &copy) = delete;
    bool operator== (const UsbTerminal &param) const = delete;
    bool operator!= (const UsbTerminal &param) const = delete;
    static TaskHandle_t usb_host_lib_task; // Reused by multiple devices or between reconnections

    static bool handle_rx(const uint8_t *data, size_t data_len, void *user_arg)
    {
        ESP_LOG_BUFFER_HEXDUMP(TAG, data, data_len, ESP_LOG_DEBUG);
        auto *this_terminal = static_cast<UsbTerminal *>(user_arg);
        if (data_len > 0 && this_terminal->on_read) {
            return this_terminal->on_read((uint8_t *)data, data_len);
        } else {
            ESP_LOGD(TAG, "Unhandled RX data");
            return true;
        }
    }

    static void handle_notif(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
    {
        auto *this_terminal = static_cast<UsbTerminal *>(user_ctx);

        switch (event->type) {
        // Notifications like Ring, Rx Carrier indication or Network connection indication are not relevant for USB terminal
        case CDC_ACM_HOST_NETWORK_CONNECTION:
        case CDC_ACM_HOST_SERIAL_STATE:
            ESP_LOGD(TAG, "Ignored USB event %d", event->type);
            break;
        case CDC_ACM_HOST_DEVICE_DISCONNECTED:
            ESP_LOGW(TAG, "USB terminal disconnected");
            if (this_terminal->on_error) {
                this_terminal->on_error(terminal_error::DEVICE_GONE);
            }
            this_terminal->close();
            break;
        case CDC_ACM_HOST_ERROR:
            ESP_LOGE(TAG, "Unexpected CDC-ACM error: %d.", event->data.error);
            if (this_terminal->on_error) {
                this_terminal->on_error(terminal_error::UNEXPECTED_CONTROL_FLOW);
            }
            break;
        default:
            abort();
        }
    };
};
TaskHandle_t UsbTerminal::usb_host_lib_task = nullptr;

std::unique_ptr<Terminal> create_usb_terminal(const esp_modem_dte_config *config, int term_idx)
{
    TRY_CATCH_RET_NULL(
        return std::make_unique<UsbTerminal>(config, term_idx);
    )
}
} // namespace esp_modem
