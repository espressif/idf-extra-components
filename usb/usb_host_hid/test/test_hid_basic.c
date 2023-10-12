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
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_private/usb_phy.h"
#include "usb/usb_host.h"

#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"

#include "test_hid_basic.h"
#include "hid_mock_device.h"

// USB PHY for device discinnection emulation
static usb_phy_handle_t phy_hdl = NULL;

// Global variable to verify user arg passing through callbacks
static uint32_t user_arg_value = 0x8A53E0A4; // Just a constant random number

// Queue and task for possibility to interact with USB device
// IMPORTANT: Interaction is not possible within device/interface callback
static bool time_to_shutdown = false;
static bool time_to_stop_polling = false;
QueueHandle_t hid_host_test_event_queue;
TaskHandle_t hid_test_task_handle;

// Multiple tasks testing
// static hid_host_device_handle_t global_hdl;
static int test_hid_device_expected;
static int test_num_passed;

static const char *test_hid_sub_class_names[] = {
    "NO_SUBCLASS",
    "BOOT_INTERFACE",
};

static const char *test_hid_proto_names[] = {
    "NONE",
    "KEYBOARD",
    "MOUSE"
};

typedef enum {
    HID_HOST_TEST_TOUCH_WAY_ASSERT = 0x00,
    HID_HOST_TEST_TOUCH_WAY_SUDDEN_DISCONNECT = 0x01,
} hid_host_test_touch_way_t;

static void force_conn_state(bool connected, TickType_t delay_ticks)
{
    TEST_ASSERT_NOT_NULL(phy_hdl);
    if (delay_ticks > 0) {
        //Delay of 0 ticks causes a yield. So skip if delay_ticks is 0.
        vTaskDelay(delay_ticks);
    }
    ESP_ERROR_CHECK(usb_phy_action(phy_hdl, (connected) ? USB_PHY_ACTION_HOST_ALLOW_CONN : USB_PHY_ACTION_HOST_FORCE_DISCONN));
}

void hid_host_test_polling_task(void *pvParameters)
{
    // Wait queue
    while (!time_to_stop_polling) {
        hid_host_handle_events(portMAX_DELAY);
    }

    vTaskDelete(NULL);
}

#define MULTIPLE_TASKS_TASKS_NUM 10

void test_class_specific_requests(hid_host_device_handle_t hid_dev_hdl)
{
    hid_host_dev_info_t hid_dev_info;
    hid_report_protocol_t proto;
    uint8_t tmp[10] = { 0 };     // for input report
    size_t rep_len = 0;
    uint8_t idle_rate;
    uint8_t rep[1] = { 0x00 };
    // Get Info
    TEST_ASSERT_EQUAL(ESP_OK, hid_host_get_device_info(hid_dev_hdl, &hid_dev_info));

    // Get Report Descriptor
    uint8_t *hid_report_descriptor = malloc(hid_dev_info.wReportDescriptorLenght);
    size_t length = 0;
    TEST_ASSERT_EQUAL(ESP_OK, hid_host_get_report_descriptor(hid_dev_hdl,
                      hid_report_descriptor,
                      hid_dev_info.wReportDescriptorLenght,
                      &length));
    free(hid_report_descriptor);

    // Get Protocol
    TEST_ASSERT_EQUAL(ESP_OK, hid_class_request_get_protocol(hid_dev_hdl, &proto));

    switch (hid_dev_info.bProtocol) {
    case HID_PROTOCOL_KEYBOARD:
        // Get Idle
        TEST_ASSERT_EQUAL(ESP_OK, hid_class_request_get_idle(hid_dev_hdl, 0, &idle_rate));
        // Set Idle
        TEST_ASSERT_EQUAL(ESP_OK, hid_class_request_set_idle(hid_dev_hdl, 0, 0));
        // Set Report
        TEST_ASSERT_EQUAL(ESP_OK, hid_class_request_set_report(hid_dev_hdl,
                          HID_REPORT_TYPE_OUTPUT,
                          0x01,
                          rep,
                          1));
        // Get Report
        rep_len = sizeof(tmp);
        TEST_ASSERT_EQUAL(ESP_OK, hid_class_request_get_report(hid_dev_hdl,
                          HID_REPORT_TYPE_INPUT,
                          0x01,
                          tmp,
                          &rep_len));
        // Set Protocol
        TEST_ASSERT_EQUAL(ESP_OK, hid_class_request_set_protocol(hid_dev_hdl, HID_REPORT_PROTOCOL_BOOT));
        break;

    case HID_PROTOCOL_MOUSE:
        // Get Report
        rep_len = sizeof(tmp);
        TEST_ASSERT_EQUAL(ESP_OK, hid_class_request_get_report(hid_dev_hdl,
                          HID_REPORT_TYPE_INPUT,
                          0x02,
                          tmp,
                          &rep_len));
        // Set Protocol
        TEST_ASSERT_EQUAL(ESP_OK, hid_class_request_set_protocol(hid_dev_hdl, HID_REPORT_PROTOCOL_BOOT));
        break;

    default: /* HID_PROTOCOL_NONE */
        rep_len = sizeof(tmp);
        TEST_ASSERT_EQUAL(ESP_OK, hid_class_request_get_report(hid_dev_hdl,
                          HID_REPORT_TYPE_INPUT,
                          0x01,
                          tmp,
                          &rep_len));
        rep_len = sizeof(tmp);
        TEST_ASSERT_EQUAL(ESP_OK, hid_class_request_get_report(hid_dev_hdl,
                          HID_REPORT_TYPE_INPUT,
                          0x02,
                          tmp,
                          &rep_len));
        break;
    }
}

void concurrent_task(void *arg)
{
    test_class_specific_requests((hid_host_device_handle_t) arg);
    test_num_passed++;
    vTaskDelete(NULL);
}

void get_report_task(void *arg)
{
    hid_host_device_handle_t hid_dev_hdl = (hid_host_device_handle_t) arg;
    hid_report_protocol_t proto;

    while (1) {
        if (ESP_OK != hid_class_request_get_protocol(hid_dev_hdl, &proto)) {
            printf("Get Protocol return error");
            break;
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief Start USB Host and handle common USB host library events while devices/clients are present
 *
 * @param[in] arg  Main task handle
 */
static void usb_lib_task(void *arg)
{
    // Initialize the internal USB PHY to connect to the USB OTG peripheral.
    // We manually install the USB PHY for testing
    usb_phy_config_t phy_config = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_HOST,
        .otg_speed = USB_PHY_SPEED_UNDEFINED,   //In Host mode, the speed is determined by the connected device
    };
    TEST_ASSERT_EQUAL(ESP_OK, usb_new_phy(&phy_config, &phy_hdl));

    const usb_host_config_t host_config = {
        .skip_phy_setup = true,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    TEST_ASSERT_EQUAL(ESP_OK, usb_host_install(&host_config));
    printf("USB Host installed\n");
    xTaskNotifyGive(arg);

    bool all_clients_gone = false;
    bool all_dev_free = false;
    while (!all_clients_gone || !all_dev_free) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

        // Release devices once all clients has deregistered
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
            printf("USB Event flags: NO_CLIENTS\n");
            all_clients_gone = true;
        }
        // All devices were removed
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            printf("USB Event flags: ALL_FREE\n");
            all_dev_free = true;
        } else {
            usb_host_lib_info_t info;
            TEST_ASSERT_EQUAL(ESP_OK, usb_host_lib_info(&info));
            if (info.num_devices == 0) {
                all_dev_free = true;
            }
        }
    }
    // Notify that device was being disconnected
    xTaskNotifyGive(arg);

    // Change global flag for all tasks still running
    time_to_shutdown = true;

    // Clean up USB Host
    vTaskDelay(pdMS_TO_TICKS(10)); // Short delay to allow clients clean-up
    TEST_ASSERT_EQUAL(ESP_OK, usb_host_uninstall());
    TEST_ASSERT_EQUAL(ESP_OK, usb_del_phy(phy_hdl)); //Tear down USB PHY
    phy_hdl = NULL;
    vTaskDelete(NULL);
}

static void hid_host_event_cb_regular(void *handler_args,
                                      esp_event_base_t base,
                                      int32_t id,
                                      void *event_data)
{
    hid_host_event_t event = (hid_host_event_t)id;
    hid_host_event_data_t *param = (hid_host_event_data_t *)event_data;

    switch (event) {
    case HID_HOST_CONNECT_EVENT: {
        hid_host_device_open(&param->connect.usb);
        break;
    }

    case HID_HOST_OPEN_EVENT: {
        hid_host_device_enable_input(param->open.dev);
        break;
    }

    case HID_HOST_DISCONNECT_EVENT: {
        hid_host_device_close(param->disconnect.dev);
        break;
    }
    case HID_HOST_INPUT_EVENT: {
        break;
    }

    default:
        printf("HID Host unhandled event: %d\n", event);
        break;
    }
}

static void hid_host_event_cb_get_info(void *handler_args,
                                       esp_event_base_t base,
                                       int32_t id,
                                       void *event_data)
{
    hid_host_event_t event = (hid_host_event_t)id;
    hid_host_event_data_t *param = (hid_host_event_data_t *)event_data;

    switch (event) {
    case HID_HOST_CONNECT_EVENT: {
        hid_host_device_open(&param->connect.usb);
        break;
    }

    case HID_HOST_OPEN_EVENT: {
        hid_host_dev_info_t hid_dev_info;
        TEST_ASSERT_EQUAL(ESP_OK, hid_host_get_device_info(param->open.dev,
                          &hid_dev_info));

        printf("\t VID: 0x%04X\n", hid_dev_info.VID);
        printf("\t PID: 0x%04X\n", hid_dev_info.PID);
        wprintf(L"\t iProduct: %S \n", hid_dev_info.iProduct);
        wprintf(L"\t iManufacturer: %S \n", hid_dev_info.iManufacturer);
        wprintf(L"\t iSerialNumber: %S \n", hid_dev_info.iSerialNumber);
        printf("\t InterfaceNum: %d\n", hid_dev_info.bInterfaceNum);
        printf("\t SubClass: '%s'\n", test_hid_sub_class_names[hid_dev_info.bSubClass]);
        printf("\t Protocol: '%s'\n", test_hid_proto_names[hid_dev_info.bProtocol]);
        printf("\t Report Descriptor Length: %d Byte(s) \n", hid_dev_info.wReportDescriptorLenght);
        printf("\t CoutryCode: 0x%02X \n", hid_dev_info.bCountryCode);

        // Get Report Descriptor
        uint8_t *hid_report_descriptor = malloc(hid_dev_info.wReportDescriptorLenght);
        size_t length = 0;
        hid_host_get_report_descriptor(param->open.dev,
                                       hid_report_descriptor,
                                       hid_dev_info.wReportDescriptorLenght,
                                       &length);
        // Print report descriptor
        for (uint8_t i = 0; i < length; ++i) {
            printf("%02X ", hid_report_descriptor[i]);
            if ((i % 0x10) == 0xf) {
                printf("\n");
            }
        }
        if (length % 0x10) {
            printf("\n");
        }
        //
        free(hid_report_descriptor);
        hid_host_device_enable_input(param->open.dev);
        break;
    }

    case HID_HOST_DISCONNECT_EVENT: {
        hid_host_device_close(param->disconnect.dev);
        break;
    }

    case HID_HOST_INPUT_EVENT: {
        printf("HID Host input report\n");
        for (int i = 0; i < param->input.length; i++) {
            printf("%02X ", param->input.data[i]);
        }
        printf("\n");
        break;
    }

    default:
        printf("HID Host unhandled event: %d\n", event);
        break;
    }
}

static void hid_host_event_cb_concurrent(void *handler_args,
        esp_event_base_t base,
        int32_t id,
        void *event_data)
{
    hid_host_event_t event = (hid_host_event_t)id;
    hid_host_event_data_t *param = (hid_host_event_data_t *)event_data;

    switch (event) {
    case HID_HOST_CONNECT_EVENT: {
        hid_host_device_open(&param->connect.usb);
        test_hid_device_expected++;
        break;
    }

    case HID_HOST_OPEN_EVENT: {
        hid_host_device_enable_input(param->open.dev);
        // Create tasks that will try to access HID Device
        for (int i = 0; i < MULTIPLE_TASKS_TASKS_NUM; i++) {
            TEST_ASSERT_EQUAL(pdTRUE,
                              xTaskCreate(concurrent_task,
                                          "HID multi touch",
                                          4096,
                                          (void *) param->open.dev,
                                          i + 3,
                                          NULL));
        }
        break;
    }

    case HID_HOST_DISCONNECT_EVENT: {
        hid_host_device_close(param->disconnect.dev);
        break;
    }
    case HID_HOST_INPUT_EVENT: {
        break;
    }

    default:
        printf("HID Host unhandled event: %d\n", event);
        break;
    }
}

static void hid_host_event_cb_class_specific(void *handler_args,
        esp_event_base_t base,
        int32_t id,
        void *event_data)
{
    hid_host_event_t event = (hid_host_event_t)id;
    hid_host_event_data_t *param = (hid_host_event_data_t *)event_data;

    switch (event) {
    case HID_HOST_CONNECT_EVENT: {
        hid_host_device_open(&param->connect.usb);
        break;
    }

    case HID_HOST_OPEN_EVENT: {
        hid_host_device_enable_input(param->open.dev);
        test_class_specific_requests(param->open.dev);
        break;
    }

    case HID_HOST_DISCONNECT_EVENT: {
        hid_host_device_close(param->disconnect.dev);
        break;
    }
    case HID_HOST_INPUT_EVENT: {
        break;
    }

    default:
        printf("HID Host unhandled event: %d\n", event);
        break;
    }
}

static void hid_host_event_cb_sudden_disconnect(void *handler_args,
        esp_event_base_t base,
        int32_t id,
        void *event_data)
{
    hid_host_event_t event = (hid_host_event_t)id;
    hid_host_event_data_t *param = (hid_host_event_data_t *)event_data;

    switch (event) {
    case HID_HOST_CONNECT_EVENT: {
        hid_host_device_open(&param->connect.usb);
        break;
    }

    case HID_HOST_OPEN_EVENT: {
        hid_host_device_enable_input(param->open.dev);
        // Create tasks that will try to access HID Device
        TEST_ASSERT_EQUAL(pdTRUE,
                          xTaskCreate(get_report_task,
                                      "HID Device Get Report",
                                      4096,
                                      (void *) param->open.dev,
                                      3,
                                      NULL));
        break;
    }

    case HID_HOST_DISCONNECT_EVENT: {
        hid_host_device_close(param->disconnect.dev);
        break;
    }
    case HID_HOST_INPUT_EVENT: {
        break;
    }

    default:
        printf("HID Host unhandled event: %d\n", event);
        break;
    }
}

static void hid_host_event_cb_out_ep(void *handler_args,
                                     esp_event_base_t base,
                                     int32_t id,
                                     void *event_data)
{
    hid_host_event_t event = (hid_host_event_t)id;
    hid_host_event_data_t *param = (hid_host_event_data_t *)event_data;

    switch (event) {
    case HID_HOST_CONNECT_EVENT: {
        hid_host_device_open(&param->connect.usb);
        break;
    }

    case HID_HOST_OPEN_EVENT: {
        hid_host_device_enable_input(param->open.dev);
        // Send OUT report
        uint8_t data[64] = { 0 };
        TEST_ASSERT_EQUAL(ESP_OK, hid_host_device_output(param->open.dev,
                          data,
                          64));
        break;
    }

    case HID_HOST_DISCONNECT_EVENT: {
        hid_host_device_close(param->disconnect.dev);
        break;
    }

    case HID_HOST_INPUT_EVENT: {
        printf("HID Host input report\n");
        for (int i = 0; i < param->input.length; i++) {
            printf("%02X ", param->input.data[i]);
        }
        printf("\n");
        break;
    }

    default:
        printf("HID Host unhandled event: %d\n", event);
        break;
    }
}

/**
 * @brief Setups HID testing
 *
 * - Create USB lib task
 * - Install HID Host driver
 */
void test_hid_setup(esp_event_handler_t device_callback)
{
    TEST_ASSERT_EQUAL(pdTRUE, xTaskCreatePinnedToCore(usb_lib_task,
                      "usb_events",
                      4096,
                      xTaskGetCurrentTaskHandle(),
                      2, NULL, 0));
    // Wait for notification from usb_lib_task
    ulTaskNotifyTake(false, 1000);

    // HID host driver config
    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = device_callback,
        .callback_arg = (void *) &user_arg_value
    };

    TEST_ASSERT_EQUAL(ESP_OK, hid_host_install(&hid_host_driver_config));
}

/**
 * @brief Teardowns HID testing
 * - Disconnect connected USB device manually by PHY triggering
 * - Wait for USB lib task was closed
 * - Uninstall HID Host driver
 * - Clear the notification value to 0
 * - Short delay to allow task to be cleaned up
 */
void test_hid_teardown(void)
{
    force_conn_state(false, pdMS_TO_TICKS(1000));
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(ESP_OK, hid_host_uninstall());
    ulTaskNotifyValueClear(NULL, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
}

// ------------------------- HID Test ------------------------------------------
#if (0)
static void test_setup_hid_polling_task(void)
{
    time_to_stop_polling = false;

    TEST_ASSERT_EQUAL(pdTRUE, xTaskCreate(&hid_host_test_polling_task,
                                          "hid_task_polling",
                                          4 * 1024,
                                          NULL, 2, NULL));
}
#endif //

TEST_CASE("memory_leakage", "[hid_host]")
{
    // Install USB and HID driver with the 'hid_host_event_cb_regular'
    test_hid_setup(hid_host_event_cb_regular);
    // Tear down test
    test_hid_teardown();
    // Verify the memory leakage during test environment tearDown()
}

TEST_CASE("device_info", "[hid_host]")
{
    // Install USB and HID driver with 'hid_host_event_cb_get_info'
    test_hid_setup(hid_host_event_cb_get_info);
    // Tear down test
    test_hid_teardown();
    // Verify the memory leakage during test environment tearDown()
}

TEST_CASE("multiple_task_access", "[hid_host]")
{
    // Refresh the num passed test value
    test_hid_device_expected = 0;
    test_num_passed = 0;
    // Install USB and HID driver with 'hid_host_event_cb_concurrent'
    test_hid_setup(hid_host_event_cb_concurrent);
    // Wait until all tasks finish
    vTaskDelay(pdMS_TO_TICKS(500));
    // Tear down test
    test_hid_teardown();
    // Verify how much tests was done
    TEST_ASSERT_EQUAL(test_hid_device_expected * MULTIPLE_TASKS_TASKS_NUM,
                      test_num_passed);
    // Verify the memory leakage during test environment tearDown()
}

TEST_CASE("class_specific_requests", "[hid_host]")
{
    // Install USB and HID driver with 'hid_host_test_device_callback_to_queue'
    test_hid_setup(hid_host_event_cb_class_specific);
    // Wait for test completed for 250 ms
    vTaskDelay(pdMS_TO_TICKS(250));
    // Tear down test
    test_hid_teardown();
    // Verify the memory leakage during test environment tearDown()
}

#if (0)
TEST_CASE("class_specific_requests_with_external_polling", "[hid_host]")
{
    // Install USB and HID driver with 'hid_host_test_device_callback_to_queue'
    test_hid_setup(hid_host_test_device_callback_to_queue, HID_TEST_EVENT_HANDLE_EXTERNAL);
    // Create HID Driver events polling task
    test_setup_hid_polling_task();
    // Wait for test completed for 250 ms
    vTaskDelay(250);
    // Tear down test
    test_hid_teardown();
    // Verify the memory leakage during test environment tearDown()
}

TEST_CASE("class_specific_requests_with_external_polling_without_polling", "[hid_host]")
{
    // Create external HID events task
    test_setup_hid_task();
    // Install USB and HID driver with 'hid_host_test_device_callback_to_queue'
    test_hid_setup(hid_host_test_device_callback_to_queue, HID_TEST_EVENT_HANDLE_EXTERNAL);
    // Do not create HID Driver events polling task to eliminate events polling
    // ...
    // Wait for 250 ms
    vTaskDelay(250);
    // Tear down test
    test_hid_teardown();
    // Verify the memory leakage during test environment tearDown()
}
#endif //

TEST_CASE("sudden_disconnect", "[hid_host]")
{
    // Install USB and HID driver with 'hid_host_event_cb_sudden_disconnect'
    test_hid_setup(hid_host_event_cb_sudden_disconnect);
    // Tear down test during thr task_access stress the HID device
    test_hid_teardown();
}

TEST_CASE("output_endpoint", "[hid_host][ignore]")
{
    // Install USB and HID driver with 'hid_host_test_device_callback_to_queue'
    test_hid_setup(hid_host_event_cb_out_ep);
    // Wait for test completed for 250 ms
    vTaskDelay(pdMS_TO_TICKS(250));
    // Tear down test
    test_hid_teardown();
    // Verify the memory leakage during test environment tearDown()
}

TEST_CASE("manual_connection", "[hid_host][ignore]")
{
    // Install USB and HID driver with the regular hid_host_test_callback
    test_hid_setup(hid_host_event_cb_get_info);
    // Wait for USB HID device plug in manually
    vTaskDelay(pdMS_TO_TICKS(5 * 1000));
    // Tear down test
    test_hid_teardown();
    // Verify the memory leakage during test environment tearDown()
}

TEST_CASE("mock_hid_device", "[hid_device][ignore]")
{
    hid_mock_device(TUSB_IFACE_COUNT_ONE);
    while (1) {
        vTaskDelay(10);
    }
}

TEST_CASE("mock_hid_device_with_two_ifaces", "[hid_device2][ignore]")
{
    hid_mock_device(TUSB_IFACE_COUNT_TWO);
    while (1) {
        vTaskDelay(10);
    }
}
