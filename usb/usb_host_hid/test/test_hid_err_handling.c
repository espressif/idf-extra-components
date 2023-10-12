/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/hid_host.h"

#include "test_hid_basic.h"

// ----------------------- Private -------------------------
/**
 * @brief USB HID Host interface callback.
 *
 * Handle close event only.
 *
 * @param[in] event  HID Host device event
 * @param[in] arg    Pointer to arguments, does not used
 *
 */
#if (0)
static void test_hid_host_interface_event_close(hid_host_device_handle_t hid_device_handle,
        const hid_host_interface_event_t event,
        void *arg)
{
    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        TEST_ASSERT_EQUAL(ESP_OK, hid_host_device_close(hid_device_handle));
        break;
    }
}
#endif //

/**
 * @brief USB HID Host event callback stub.
 *
 * Does not handle anything.
 *
 * @param[in] handler_args
 * @param[in] base
 * @param[in] id
 * @param[in] event_data
 */
static void hid_host_event_cb_stub(void *handler_args,
                                   esp_event_base_t base,
                                   int32_t id,
                                   void *event_data)
{
    hid_host_event_t event = (hid_host_event_t)id;
    switch (event) {
    case HID_HOST_CONNECT_EVENT:
    case HID_HOST_OPEN_EVENT:
    case HID_HOST_DISCONNECT_EVENT:
    case HID_HOST_INPUT_EVENT:
    default:
        printf("HID Host stub event: %d\n", event);
        break;
    }
}

static void hid_host_event_cb_open_close(void *handler_args,
        esp_event_base_t base,
        int32_t id,
        void *event_data)
{
    hid_host_event_t event = (hid_host_event_t)id;
    hid_host_event_data_t *param = (hid_host_event_data_t *)event_data;
    switch (event) {
    case HID_HOST_CONNECT_EVENT:
        TEST_ASSERT_EQUAL(ESP_OK, hid_host_device_open(&param->connect.usb));
        break;
    case HID_HOST_OPEN_EVENT:
        TEST_ASSERT_EQUAL(ESP_OK, hid_host_device_enable_input(param->open.dev));
        break;
    case HID_HOST_DISCONNECT_EVENT:
        TEST_ASSERT_EQUAL(ESP_OK, hid_host_device_close(param->disconnect.dev));
        break;
    case HID_HOST_INPUT_EVENT:
    default:
        printf("HID Host unhandled event: %d\n", event);
        break;
    }
}

// Install HID driver without USB Host and without configuration
static void test_install_hid_driver_without_config(void)
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, hid_host_install(NULL));
}


// Install HID driver without USB Host and with configuration
static void test_install_hid_driver_with_wrong_config(void)
{
    const hid_host_driver_config_t hid_host_config_callback_null = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = NULL, /* error expected */
        .callback_arg = NULL
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      hid_host_install(&hid_host_config_callback_null));

    const hid_host_driver_config_t hid_host_config_stack_size_null = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 0, /* error expected */
        .core_id = 0,
        .callback = hid_host_event_cb_stub,
        .callback_arg = NULL
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      hid_host_install(&hid_host_config_stack_size_null));

    const hid_host_driver_config_t hid_host_config_task_priority_null = {
        .create_background_task = true,
        .task_priority = 0,/* error expected */
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_event_cb_stub,
        .callback_arg = NULL
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      hid_host_install(&hid_host_config_task_priority_null));

    const hid_host_driver_config_t hid_host_config_correct = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_event_cb_stub,
        .callback_arg = NULL
    };
    // Invalid state without USB Host installed
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      hid_host_install(&hid_host_config_correct));
}

// Open device without installed driver
static void test_device_api_without_driver(void)
{
    hid_host_device_handle_t hid_dev_handle = NULL;
    hid_host_dev_params_t dev_params = {
        .addr = 0x01,
        .iface_num = 0x00,
        .proto = HID_PROTOCOL_NONE,
        .sub_class = HID_SUBCLASS_NO_SUBCLASS
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      hid_host_device_open(&dev_params));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      hid_host_device_close(hid_dev_handle));

    // hid_host_get_device_info
    // hid_host_handle_events
    // hid_host_device_enable_input
    // hid_host_device_disable_input
    // hid_host_device_output
    // hid_host_get_report_descriptor
    // hid_host_get_device_info
    // hid_class_request_get_report
    // hid_class_request_get_idle
    // hid_class_request_get_protocol
    // hid_class_request_set_report
    // hid_class_request_set_idle
    // hid_class_request_set_protocol
}

static void test_install_hid_driver_when_already_installed(void)
{
    // Install USB and HID driver with the stub 'hid_host_event_cb_stub'
    test_hid_setup(hid_host_event_cb_stub,
                   HID_TEST_EVENT_HANDLE_TYPE_DRIVER);
    // Try to install HID driver again
    const hid_host_driver_config_t hid_host_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_event_cb_stub,
        .callback_arg = NULL
    };
    // Verify error code
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, hid_host_install(&hid_host_config));
    // Tear down test
    test_hid_teardown();
}

static void test_uninstall_hid_driver_while_device_was_not_opened(void)
{
    // Install USB and HID driver with the stub 'hid_host_event_cb_stub'
    test_hid_setup(hid_host_event_cb_stub,
                   HID_TEST_EVENT_HANDLE_TYPE_DRIVER);
    // Tear down test
    test_hid_teardown();
}

static void test_uninstall_hid_driver_while_device_is_present(void)
{
    // Install USB and HID driver with the stub 'hid_host_event_cb_stub'
    test_hid_setup(hid_host_event_cb_open_close,
                   HID_TEST_EVENT_HANDLE_TYPE_DRIVER);
    // Wait until device appears
    vTaskDelay(pdMS_TO_TICKS(500));
    // Try uninstall hid driver
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, hid_host_uninstall());
    // Tear down test
    test_hid_teardown();
}

// ----------------------- Public --------------------------

/**
 * @brief HID Error handling test
 *
 * There are multiple erroneous scenarios checked in this test.
 *
 */
TEST_CASE("error_handling", "[hid_host]")
{
    test_install_hid_driver_without_config();
    test_install_hid_driver_with_wrong_config();
    test_device_api_without_driver();
    test_install_hid_driver_when_already_installed();
    test_uninstall_hid_driver_while_device_was_not_opened();
    test_uninstall_hid_driver_while_device_is_present();
}
