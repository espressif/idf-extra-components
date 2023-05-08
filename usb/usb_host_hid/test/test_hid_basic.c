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
static uint32_t user_arg_value = 0x8A53E0A4; // Just a constant renadom number

// Queue and task for possibility to interact with USB device
// IMPORTANT: Interaction is not possible within device/interface callback
static bool time_to_shutdown = false;
QueueHandle_t hid_host_test_event_queue;
TaskHandle_t hid_test_task_handle;

// Multiple tasks testing
static hid_host_device_handle_t global_hdl;
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

typedef struct {
    hid_host_device_handle_t hid_device_handle;
    hid_host_driver_event_t event;
    void *arg;
} hid_host_test_event_queue_t;

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

void hid_host_test_interface_callback(hid_host_device_handle_t hid_device_handle,
                                      const hid_host_interface_event_t event,
                                      void *arg)
{
    uint8_t data[64] = { 0 };
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;
    TEST_ASSERT_EQUAL(ESP_OK, hid_host_device_get_params(hid_device_handle, &dev_params));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&user_arg_value, arg, "User argument has lost");

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        printf("USB port %d, Interface num %d: ",
               dev_params.addr,
               dev_params.iface_num);

        hid_host_device_get_raw_input_report_data(hid_device_handle,
                data,
                64,
                &data_length);

        for (int i = 0; i < data_length; i++) {
            printf("%02x ", data[i]);
        }
        printf("\n");
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        printf("USB port %d, iface num %d removed\n",
               dev_params.addr,
               dev_params.iface_num);
        TEST_ASSERT_EQUAL(ESP_OK, hid_host_device_close(hid_device_handle) );
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        printf("USB Host transfer error\n");
        break;
    default:
        TEST_FAIL_MESSAGE("HID Interface unhandled event");
        break;
    }
}

void hid_host_test_callback(hid_host_device_handle_t hid_device_handle,
                            const hid_host_driver_event_t event,
                            void *arg)
{
    hid_host_dev_params_t dev_params;
    TEST_ASSERT_EQUAL(ESP_OK, hid_host_device_get_params(hid_device_handle, &dev_params));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&user_arg_value, arg, "User argument has lost");

    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
        printf("USB port %d, interface %d, '%s', '%s'\n",
               dev_params.addr,
               dev_params.iface_num,
               test_hid_sub_class_names[dev_params.sub_class],
               test_hid_proto_names[dev_params.proto]);

        const hid_host_device_config_t dev_config = {
            .callback = hid_host_test_interface_callback,
            .callback_arg = (void *) &user_arg_value
        };

        TEST_ASSERT_EQUAL(ESP_OK,  hid_host_device_open(hid_device_handle, &dev_config) );
        TEST_ASSERT_EQUAL(ESP_OK,  hid_host_device_start(hid_device_handle) );
        break;
    default:
        TEST_FAIL_MESSAGE("HID Driver unhandled event");
        break;
    }
}

void hid_host_test_concurrent(hid_host_device_handle_t hid_device_handle,
                              const hid_host_driver_event_t event,
                              void *arg)
{
    hid_host_dev_params_t dev_params;
    TEST_ASSERT_EQUAL(ESP_OK, hid_host_device_get_params(hid_device_handle, &dev_params));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&user_arg_value, arg, "User argument has lost");

    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
        printf("USB port %d, interface %d, '%s', '%s'\n",
               dev_params.addr,
               dev_params.iface_num,
               test_hid_sub_class_names[dev_params.sub_class],
               test_hid_proto_names[dev_params.proto]);

        const hid_host_device_config_t dev_config = {
            .callback = hid_host_test_interface_callback,
            .callback_arg = &user_arg_value
        };

        TEST_ASSERT_EQUAL(ESP_OK,  hid_host_device_open(hid_device_handle, &dev_config) );
        TEST_ASSERT_EQUAL(ESP_OK,  hid_host_device_start(hid_device_handle) );

        global_hdl = hid_device_handle;
        break;
    default:
        TEST_FAIL_MESSAGE("HID Driver unhandled event");
        break;
    }
}

void hid_host_test_device_callback_to_queue(hid_host_device_handle_t hid_device_handle,
        const hid_host_driver_event_t event,
        void *arg)
{
    const hid_host_test_event_queue_t evt_queue = {
        .hid_device_handle = hid_device_handle,
        .event = event,
        .arg = arg
    };
    xQueueSend(hid_host_test_event_queue, &evt_queue, 0);
}

void hid_host_test_requests_callback(hid_host_device_handle_t hid_device_handle,
                                     const hid_host_driver_event_t event,
                                     void *arg)
{
    hid_host_dev_params_t dev_params;
    TEST_ASSERT_EQUAL(ESP_OK, hid_host_device_get_params(hid_device_handle, &dev_params));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&user_arg_value, arg, "User argument has lost");

    uint8_t *test_buffer = NULL; // for report descriptor
    unsigned int test_length = 0;
    uint8_t tmp[10] = { 0 };     // for input report
    size_t rep_len = 0;

    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
        printf("USB port %d, interface %d, '%s', '%s'\n",
               dev_params.addr,
               dev_params.iface_num,
               test_hid_sub_class_names[dev_params.sub_class],
               test_hid_proto_names[dev_params.proto]);

        const hid_host_device_config_t dev_config = {
            .callback = hid_host_test_interface_callback,
            .callback_arg = &user_arg_value
        };

        TEST_ASSERT_EQUAL(ESP_OK,  hid_host_device_open(hid_device_handle, &dev_config) );

        // Class device requests
        // hid_host_get_report_descriptor
        test_buffer = hid_host_get_report_descriptor(hid_device_handle, &test_length);

        if (test_buffer) {
            printf("HID Report descriptor length: %d\n", test_length);
        }

        if (dev_params.proto == HID_PROTOCOL_NONE) {
            // If Protocol NONE, based on hid1_11.pdf, p.78, all ohter devices should support
            rep_len = sizeof(tmp);
            // For testing with ESP32 we used ReportID = 0x01 (Keyboard ReportID)
            if (ESP_OK == hid_class_request_get_report(hid_device_handle,
                    HID_REPORT_TYPE_INPUT, 0x01, tmp, &rep_len)) {
                printf("HID Get Report, type %d, id %d, length: %d:\n",
                       HID_REPORT_TYPE_INPUT, 0, rep_len);
                for (int i = 0; i < rep_len; i++) {
                    printf("%02X ", tmp[i]);
                }
                printf("\n");
            }

            rep_len = sizeof(tmp);
            // For testing with ESP32 we used ReportID = 0x02 (Mouse ReportID)
            if (ESP_OK == hid_class_request_get_report(hid_device_handle,
                    HID_REPORT_TYPE_INPUT, 0x02, tmp, &rep_len)) {
                printf("HID Get Report, type %d, id %d, length: %d:\n",
                       HID_REPORT_TYPE_INPUT, 0, rep_len);
                for (int i = 0; i < rep_len; i++) {
                    printf("%02X ", tmp[i]);
                }
                printf("\n");
            }
        } else {
            // hid_class_request_get_protocol
            hid_report_protocol_t proto;
            if (ESP_OK == hid_class_request_get_protocol(hid_device_handle, &proto)) {
                printf("HID protocol: %d\n", proto);
            }

            if (dev_params.proto == HID_PROTOCOL_KEYBOARD) {
                uint8_t idle_rate;
                // hid_class_request_get_idle
                if (ESP_OK == hid_class_request_get_idle(hid_device_handle,
                        0, &idle_rate)) {
                    printf("HID idle rate: %d\n", idle_rate);
                }
                // hid_class_request_set_idle
                if (ESP_OK == hid_class_request_set_idle(hid_device_handle,
                        0, 0)) {
                    printf("HID idle rate set to 0\n");
                }

                // hid_class_request_get_report
                rep_len = sizeof(tmp);
                if (ESP_OK == hid_class_request_get_report(hid_device_handle,
                        HID_REPORT_TYPE_INPUT, 0x01, tmp, &rep_len)) {
                    printf("HID get report type %d, id %d, length: %d\n",
                           HID_REPORT_TYPE_INPUT, 0x00, rep_len);
                }

                // hid_class_request_set_report
                uint8_t rep[1] = { 0x00 };
                if (ESP_OK == hid_class_request_set_report(hid_device_handle,
                        HID_REPORT_TYPE_OUTPUT, 0x01, rep, 1)) {
                    printf("HID set report type %d, id %d\n", HID_REPORT_TYPE_OUTPUT, 0x00);
                }
            }

            if (dev_params.proto == HID_PROTOCOL_MOUSE) {
                // hid_class_request_get_report
                rep_len = sizeof(tmp);
                if (ESP_OK == hid_class_request_get_report(hid_device_handle,
                        HID_REPORT_TYPE_INPUT, 0x02, tmp, &rep_len)) {
                    printf("HID get report type %d, id %d, length: %d\n",
                           HID_REPORT_TYPE_INPUT, 0x00, rep_len);
                }
            }

            // hid_class_request_set_protocol
            if (ESP_OK == hid_class_request_set_protocol(hid_device_handle,
                    HID_REPORT_PROTOCOL_BOOT)) {
                printf("HID protocol change to BOOT: %d\n", proto);
            }
        }

        TEST_ASSERT_EQUAL(ESP_OK,  hid_host_device_start(hid_device_handle) );
        break;
    default:
        TEST_FAIL_MESSAGE("HID Driver unhandled event");
        break;
    }
}

void hid_host_test_task(void *pvParameters)
{
    hid_host_test_event_queue_t evt_queue;
    // Create queue
    hid_host_test_event_queue = xQueueCreate(10, sizeof(hid_host_test_event_queue_t));

    // Wait queue
    while (!time_to_shutdown) {
        if (xQueueReceive(hid_host_test_event_queue, &evt_queue, pdMS_TO_TICKS(50))) {
            hid_host_test_requests_callback(evt_queue.hid_device_handle,
                                            evt_queue.event,
                                            evt_queue.arg);
        }
    }

    xQueueReset(hid_host_test_event_queue);
    vQueueDelete(hid_host_test_event_queue);
    vTaskDelete(NULL);
}

static void test_hid_host_device_touch(hid_host_dev_params_t *dev_params,
                                       hid_host_test_touch_way_t touch_way)
{
    uint8_t tmp[10] = { 0 };     // for input report
    size_t rep_len = 0;
    hid_report_protocol_t proto;

    if (dev_params->proto == HID_PROTOCOL_NONE) {
        rep_len = sizeof(tmp);
        // For testing with ESP32 we used ReportID = 0x01 (Keyboard ReportID)
        if (HID_HOST_TEST_TOUCH_WAY_ASSERT == touch_way) {
            TEST_ASSERT_EQUAL(ESP_OK, hid_class_request_get_report(global_hdl,
                              HID_REPORT_TYPE_INPUT, 0x01, tmp, &rep_len));
        } else {
            hid_class_request_get_report(global_hdl,
                                         HID_REPORT_TYPE_INPUT, 0x01, tmp, &rep_len);
        }

    } else {
        // Get Protocol
        TEST_ASSERT_EQUAL(ESP_OK, hid_class_request_get_protocol(global_hdl, &proto));
        // Get Report for Keyboard protocol, ReportID = 0x00 (Boot Keyboard ReportID)
        if (dev_params->proto == HID_PROTOCOL_KEYBOARD) {
            rep_len = sizeof(tmp);
            if (HID_HOST_TEST_TOUCH_WAY_ASSERT == touch_way) {
                TEST_ASSERT_EQUAL(ESP_OK, hid_class_request_get_report(global_hdl,
                                  HID_REPORT_TYPE_INPUT, 0x01, tmp, &rep_len));
            } else {
                hid_class_request_get_report(global_hdl,
                                             HID_REPORT_TYPE_INPUT, 0x01, tmp, &rep_len);
            }
        }
        if (dev_params->proto == HID_PROTOCOL_MOUSE) {
            rep_len = sizeof(tmp);
            if (HID_HOST_TEST_TOUCH_WAY_ASSERT == touch_way) {
                TEST_ASSERT_EQUAL(ESP_OK, hid_class_request_get_report(global_hdl,
                                  HID_REPORT_TYPE_INPUT, 0x02, tmp, &rep_len));
            } else {
                hid_class_request_get_report(global_hdl,
                                             HID_REPORT_TYPE_INPUT, 0x02, tmp, &rep_len);
            }
        }
    }
}

#define MULTIPLE_TASKS_TASKS_NUM 10

void concurrent_task(void *arg)
{
    uint8_t *test_buffer = NULL;
    unsigned int test_length = 0;
    hid_host_dev_params_t dev_params;
    TEST_ASSERT_EQUAL(ESP_OK, hid_host_device_get_params(global_hdl, &dev_params));

    // Get Report descriptor
    test_buffer = hid_host_get_report_descriptor(global_hdl, &test_length);
    TEST_ASSERT_NOT_NULL(test_buffer);

    test_hid_host_device_touch(&dev_params, HID_HOST_TEST_TOUCH_WAY_ASSERT);
    test_num_passed++;

    vTaskDelete(NULL);
}

void access_task(void *arg)
{
    uint8_t *test_buffer = NULL;
    unsigned int test_length = 0;
    hid_host_dev_params_t dev_params;
    TEST_ASSERT_EQUAL(ESP_OK, hid_host_device_get_params(global_hdl, &dev_params));

    // Get Report descriptor
    test_buffer = hid_host_get_report_descriptor(global_hdl, &test_length);
    TEST_ASSERT_NOT_NULL(test_buffer);

    while (!time_to_shutdown) {
        test_hid_host_device_touch(&dev_params, HID_HOST_TEST_TOUCH_WAY_ASSERT/* HID_HOST_TEST_TOUCH_WAY_SUDDEN_DISCONNECT */);
    }

    vTaskDelete(NULL);
}

/**
 * @brief Creates MULTIPLE_TASKS_TASKS_NUM to get report descriptor and get protocol from HID device.
 * After test_num_passed - is a global static variable that increases during every successful task test.
 */
void test_multiple_tasks_access(void)
{
    // Create tasks that will try to access HID dev with global hdl
    for (int i = 0; i < MULTIPLE_TASKS_TASKS_NUM; i++) {
        TEST_ASSERT_EQUAL(pdTRUE, xTaskCreate(concurrent_task, "HID multi touch", 4096, NULL, i + 3, NULL));
    }
    // Wait until all tasks finish
    vTaskDelay(pdMS_TO_TICKS(500));
}

void test_task_access(void)
{
    // Create task which will be touching the device with control requests, while device is present
    TEST_ASSERT_EQUAL(pdTRUE, xTaskCreate(access_task, "HID touch", 4096, NULL, 3, NULL));
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
    TEST_ASSERT_EQUAL(ESP_OK, usb_host_install(&host_config) );
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
            // Notify that device was being disconnected
            xTaskNotifyGive(arg);
        }
    }

    // Change global flag for all tasks still running
    time_to_shutdown = true;

    // Clean up USB Host
    vTaskDelay(10); // Short delay to allow clients clean-up
    TEST_ASSERT_EQUAL(ESP_OK, usb_host_uninstall());
    TEST_ASSERT_EQUAL(ESP_OK, usb_del_phy(phy_hdl)); //Tear down USB PHY
    phy_hdl = NULL;
    vTaskDelete(NULL);
}

// ----------------------- Public -------------------------

/**
 * @brief Setups HID testing
 *
 * - Create USB lib task
 * - Install HID Host driver
 */
void test_hid_setup(hid_host_driver_event_cb_t device_callback)
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

    TEST_ASSERT_EQUAL(ESP_OK, hid_host_install(&hid_host_driver_config) );
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
    vTaskDelay(50);
    TEST_ASSERT_EQUAL(ESP_OK, hid_host_uninstall() );
    ulTaskNotifyValueClear(NULL, 1);
    vTaskDelay(20);
}

// ------------------------- HID Test ------------------------------------------
static void test_setup_hid_task(void)
{
    // Task is working until the devices are gone
    time_to_shutdown = false;
    // Create process
    TEST_ASSERT_EQUAL(pdTRUE, xTaskCreate(&hid_host_test_task,
                                          "hid_task",
                                          4 * 1024,
                                          NULL,
                                          2,
                                          &hid_test_task_handle));
}

TEST_CASE("memory_leakage", "[hid_host]")
{
    // Install USB and HID driver with the regular hid_host_test_callback
    test_hid_setup(hid_host_test_callback);
    // Tear down test
    test_hid_teardown();
    // Verify the memory leackage during test environment tearDown()
}

TEST_CASE("multiple_task_access", "[hid_host]")
{
    // Install USB and HID driver with 'hid_host_test_concurrent'
    test_hid_setup(hid_host_test_concurrent);
    // Wait for USB device appearing for 250 msec
    vTaskDelay(250);
    // Refresh the num passed test value
    test_num_passed = 0;
    // Start multiple task access to USB device with control requests
    test_multiple_tasks_access();
    // Tear down test
    test_hid_teardown();
    // Verify how much tests was done
    TEST_ASSERT_EQUAL(MULTIPLE_TASKS_TASKS_NUM, test_num_passed);
    // Verify the memory leackage during test environment tearDown()
}

TEST_CASE("class_specific_requests", "[hid_host]")
{
    // Create external HID events task
    test_setup_hid_task();
    // Install USB and HID driver with 'hid_host_test_device_callback_to_queue'
    test_hid_setup(hid_host_test_device_callback_to_queue);
    // All specific control requests will be verified during device connetion callback 'hid_host_test_requests_callback'
    // Wait for test completed for 250 ms
    vTaskDelay(250);
    // Tear down test
    test_hid_teardown();
    // Verify the memory leackage during test environment tearDown()
}

TEST_CASE("sudden_disconnect", "[hid_host]")
{
    // Install USB and HID driver with 'hid_host_test_concurrent'
    test_hid_setup(hid_host_test_concurrent);
    // Wait for USB device appearing for 250 msec
    vTaskDelay(250);
    // Start task to access USB device with control requests
    test_task_access();
    // Tear down test during thr task_access stress the HID device
    test_hid_teardown();
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
