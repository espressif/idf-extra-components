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
 * @param[in] event  HID Host device event
 * @param[in] arg    Pointer to arguments, does not used
 *
 */
#if (0)
static void test_hid_host_event_callback_stub(hid_host_device_handle_t hid_device_handle,
        const hid_host_driver_event_t event,
        void *arg)
{
    if (event == HID_HOST_DRIVER_EVENT_CONNECTED) {
        // Device connected
    }
}

/**
 * @brief USB HID Host event callback.
 *
 * Handle connected event and open a device.
 *
 * @param[in] event  HID Host device event
 * @param[in] arg    Pointer to arguments, does not used
 *
 */
static void test_hid_host_event_callback_open(hid_host_device_handle_t hid_device_handle,
        const hid_host_driver_event_t event,
        void *arg)
{
    if (event == HID_HOST_DRIVER_EVENT_CONNECTED) {
        const hid_host_device_config_t dev_config = {
            .callback = test_hid_host_interface_event_close,
            .callback_arg = NULL
        };

        TEST_ASSERT_EQUAL(ESP_OK,  hid_host_device_open(hid_device_handle, &dev_config));
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

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, hid_host_install(&hid_host_config_callback_null));

    const hid_host_driver_config_t hid_host_config_stack_size_null = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 0, /* error expected */
        .core_id = 0,
        .callback = test_hid_host_event_callback_stub,
        .callback_arg = NULL
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, hid_host_install(&hid_host_config_stack_size_null));

    const hid_host_driver_config_t hid_host_config_task_priority_null = {
        .create_background_task = true,
        .task_priority = 0,/* error expected */
        .stack_size = 4096,
        .core_id = 0,
        .callback = test_hid_host_event_callback_stub,
        .callback_arg = NULL
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, hid_host_install(&hid_host_config_task_priority_null));

    const hid_host_driver_config_t hid_host_config_correct = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = test_hid_host_event_callback_stub,
        .callback_arg = NULL
    };
    // Invalid state without USB Host installed
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, hid_host_install(&hid_host_config_correct));
}

void test_interface_callback_handler(hid_host_device_handle_t hid_device_handle,
                                     const hid_host_interface_event_t event,
                                     void *arg)
{
    // ...
}

// Open device without installed driver
static void test_claim_interface_without_driver(void)
{
    hid_host_device_handle_t hid_dev_handle = NULL;

    const hid_host_device_config_t dev_config = {
        .callback = test_interface_callback_handler,
        .callback_arg = NULL
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      hid_host_device_open(hid_dev_handle, &dev_config));
}

static void test_install_hid_driver_when_already_installed(void)
{
    // Install USB and HID driver with the stub test_hid_host_event_callback_stub
    test_hid_setup(test_hid_host_event_callback_stub, HID_TEST_EVENT_HANDLE_IN_DRIVER);
    // Try to install HID driver again
    const hid_host_driver_config_t hid_host_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = test_hid_host_event_callback_stub,
        .callback_arg = NULL
    };
    // Verify error code
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, hid_host_install(&hid_host_config));
    // Tear down test
    test_hid_teardown();
}

static void test_uninstall_hid_driver_while_device_was_not_opened(void)
{
    // Install USB and HID driver with the stub test_hid_host_event_callback_stub
    test_hid_setup(test_hid_host_event_callback_stub, HID_TEST_EVENT_HANDLE_IN_DRIVER);
    // Tear down test
    test_hid_teardown();
}
#endif //

// ----------------------- Public --------------------------

/**
 * @brief HID Error handling test
 *
 * There are multiple erroneous scenarios checked in this test.
 *
 */
#if (0)
TEST_CASE("error_handling", "[hid_host]")
{
    test_install_hid_driver_without_config();
    test_install_hid_driver_with_wrong_config();
    test_claim_interface_without_driver();
    test_install_hid_driver_when_already_installed();
    test_uninstall_hid_driver_while_device_was_not_opened();
    test_uninstall_hid_driver_while_device_is_present();
}
#endif //
