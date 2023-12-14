/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <wchar.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_event.h"
#include <freertos/FreeRTOS.h>

#include "hid.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB HID HOST string descriptor maximal length
 *
 * The maximum possible number of characters in an embedded string is device specific.
 * For USB devices, the maximum string length is 126 wide characters (not including the terminating NULL character).
 * This is a length, which is available to upper level application during getting information
 * of HID Device with 'hid_host_get_device_info' call.
 *
 * To decrease memory usage 32 wide characters (64 bytes per every string) is used.
*/
#define HID_STR_DESC_MAX_LENGTH           32

typedef struct iface_obj *hid_host_device_handle_t;    /**< Device Handle. Handle to a particular HID interface */

// ------------------------ USB HID Host events --------------------------------
ESP_EVENT_DECLARE_BASE(HID_HOST_EVENTS);

/**
 * @brief HIDH callback events enum
 */
typedef enum {
    HID_HOST_ANY_EVENT = ESP_EVENT_ANY_ID,          /*!< HID device any event */
    HID_HOST_CONNECT_EVENT = 0,                     /*!< HID device connected */
    HID_HOST_OPEN_EVENT,                            /*!< HID device opened */
    HID_HOST_INPUT_EVENT,                           /*!< Received HID device INPUT report */
    HID_HOST_FEATURE_EVENT,                         /*!< Received HID device FEATURE report */
    HID_HOST_DISCONNECT_EVENT,
    HID_HOST_MAX_EVENT,                             /*!< HID events end marker */
} hid_host_event_t;

/**
 * @brief USB HID Host device parameters
*/
typedef struct {
    uint8_t addr;                       /**< USB Address of connected HID device */
    uint8_t iface_num;                  /**< HID Interface Number */
    uint8_t sub_class;                  /**< HID Interface SubClass */
    uint8_t proto;                      /**< HID Interface Protocol */
} hid_host_dev_params_t;

/**
 * @brief HID device descriptor common data
*/
typedef struct {
    uint16_t VID;
    uint16_t PID;
    wchar_t iManufacturer[HID_STR_DESC_MAX_LENGTH];
    wchar_t iProduct[HID_STR_DESC_MAX_LENGTH];
    wchar_t iSerialNumber[HID_STR_DESC_MAX_LENGTH];
    uint8_t bInterfaceNum;                   /**< HID Interface Number */
    uint8_t bSubClass;                       /**< HID Interface SubClass */
    uint8_t bProtocol;                       /**< HID Interface Protocol */
    uint16_t wReportDescriptorLenght;        /**< HID Interface report descriptor size */
    uint8_t bCountryCode;                    /**< HID Interface Country Code */
} hid_host_dev_info_t;

// ------------------------ USB HID Host callbacks -----------------------------
/**
 * @brief HID Host callback parameters union
 */
typedef union {
    /**
     * @brief HID_HOST_CONNECT_EVENT
     */
    struct {
        hid_host_dev_params_t usb;              /*!< HID Device params */
    } connect;

    /**
    * @brief HID_HOST_OPEN_EVENT
    */
    struct {
        hid_host_device_handle_t dev;            /*!< HID Device handle */
    } open;

    /**
     * @brief HID_HOST_INPUT_EVENT
     */
    struct {
        hid_host_dev_params_t usb;              /*!< HID Device params */
        hid_host_device_handle_t dev;           /*!< HID Device handle */
        // esp_hid_usage_t usage;                   /*!< HID report usage */
        // uint16_t report_id;                      /*!< HID report index */
        uint16_t length;                         /*!< HID data length */
        uint8_t data[64];                        /*!< The HID report data */
    } input;

    /**
     * @brief HID_HOST_DISCONNECT_EVENT
     */
    struct {
        hid_host_device_handle_t dev;            /*!< HID Device handle */
        hid_host_dev_params_t usb;              /*!< HID Device params */
    } disconnect;

} hid_host_event_data_t;

// ----------------------------- Public ---------------------------------------
/**
 * @brief HID configuration structure.
*/
typedef struct {
    bool create_background_task;            /**< When set to true, background task handling USB events is created.
                                         Otherwise user has to periodically call hid_host_handle_events function */
    size_t task_priority;                   /**< Task priority of created background task */
    size_t stack_size;                      /**< Stack size of created background task */
    BaseType_t core_id;                     /**< Select core on which background task will run or tskNO_AFFINITY  */
    esp_event_handler_t callback;           /**< Callback invoked when HID driver event occurs. Must not be NULL. */
    void *callback_arg;                     /**< User provided argument passed to callback */
} hid_host_driver_config_t;

/**
 * @brief USB HID Host install USB Host HID Class driver
 *
 * @param[in] config configuration structure HID to create
 * @return esp_err_r
 */
esp_err_t hid_host_install(const hid_host_driver_config_t *config);

/**
 * @brief USB HID Host uninstall HID Class driver
 * @return esp_err_t
 */
esp_err_t hid_host_uninstall(void);

/**
 * @brief USB HID Host open a device with specific device parameters
 *
 * @param[in] iface_handle     Handle of the HID devive to open
 * @param[in] config           Configuration structure HID device to open
 * @return esp_err_t
 */
esp_err_t hid_host_device_open(hid_host_dev_params_t *dev_params);

/**
 * @brief USB HID Host close device
 *
 * @param[in] hid_dev_handle   Handle of the HID devive to close
 * @return esp_err_t
 */
esp_err_t hid_host_device_close(hid_host_device_handle_t hid_dev_handle);

/**
 * @brief HID Host Get device information
 *
 * @param[in] hid_dev_handle   HID Device handle
 * @param[out] hid_dev_info    Pointer to HID Device infomration structure
*/
esp_err_t hid_host_get_device_info(hid_host_device_handle_t hid_dev_handle,
                                   hid_host_dev_info_t *hid_dev_info);

/**
 * @brief HID Host USB event handler
 *
 * If HID Host install was made with create_background_task=false configuration,
 * application needs to handle USB Host events itself.
 * Do not used if HID host install was made with create_background_task=true configuration
 *
 * @param[in]  timeout  Timeout in ticks. For milliseconds, please use 'pdMS_TO_TICKS()' macros
 * @return esp_err_t
 */
esp_err_t hid_host_handle_events(uint32_t timeout);

// ------------------------ USB HID Host driver API ----------------------------

/**
 * @brief HID Host start enable input from a device by handle using IN Endpoint
 *
 * Calls a callback when the HID Interface event has occurred.
 *
 * @param[in] hid_dev_handle  HID Device handle
 * @return esp_err_t
 */
esp_err_t hid_host_device_enable_input(hid_host_device_handle_t hid_dev_handle);

/**
 * @brief HID Host disable input from device
 *
 * @param[in] hid_dev_handle  HID Device handle
 *
 * @return esp_err_t
 */
esp_err_t hid_host_device_disable_input(hid_host_device_handle_t hid_dev_handle);

/**
 * @brief HID Host dend output report using Out Endpoint
 *
 * @param[in] hid_dev_handle  HID Device handle
 * @param[in] data            Pointer to data buffer
 * @param[in] length          Data length
 *
 * @return esp_err_t
 */
esp_err_t hid_host_device_output(hid_host_device_handle_t hid_dev_handle,
                                 const uint8_t *data,
                                 const size_t length);

/**
 * @brief HID Host Get Report Descriptor
 *
 * @param[in] hid_dev_handle            HID Device handle
 * @param[out] data                     HID Report Descriptor data pointer
 * @param[in] max_length                Data buffer maximal length
 * @param[out] length                   HID Report descriptor length
 *
 * @return esp_err_t
 */
esp_err_t hid_host_get_report_descriptor(hid_host_device_handle_t hid_dev_handle,
        uint8_t *data,
        size_t max_length,
        size_t *length);

/**
 * @brief HID Host Get device information
 *
 * @param[in] hid_dev_handle   HID Device handle
*/
esp_err_t hid_host_get_device_info(hid_host_device_handle_t hid_dev_handle,
                                   hid_host_dev_info_t *hid_dev_info);

/**
 * @brief HID class specific request GET REPORT
 *
 * @param[in] hid_dev_handle    HID Device handle
 * @param[in] report_id         Report ID
 * @param[out] report           Pointer to buffer for a report data
 * @param[in/out] report_length Report data length, before the get report contain the maximum value of a report buffer.
 * After get report there is a value of actual data in report buffer.
 *
 * @return esp_err_t
 */
esp_err_t hid_class_request_get_report(hid_host_device_handle_t hid_dev_handle,
                                       uint8_t report_type,
                                       uint8_t report_id,
                                       uint8_t *report,
                                       size_t *report_length);

/**
 * @brief HID class specific request GET IDLE
 *
 * @param[in] hid_dev_handle    HID Device handle
 * @param[in] report_id         ReportID
 * @param[out] idle_rate        Idle rate [ms]
 *
 * @return esp_err_t
 */
esp_err_t hid_class_request_get_idle(hid_host_device_handle_t hid_dev_handle,
                                     uint8_t report_id,
                                     uint8_t *idle_rate);

/**
 * @brief HID class specific request GET PROTOCOL
 *
 * @param[in] hid_dev_handle    HID Device handle
 * @param[out] protocol         Pointer to HID report protocol (boot or report) of device
 *
 * @return esp_err_t
 */
esp_err_t hid_class_request_get_protocol(hid_host_device_handle_t hid_dev_handle,
        hid_report_protocol_t *protocol);

/**
* @brief HID class specific request SET REPORT
*
* @param[in] hid_dev_handle     HID Device handle
* @param[in] report_type        Report type
* @param[in] report_id          Report ID
* @param[in] report             Pointer to a buffer with report data
* @param[in] report_length      Report data length
*
* @return esp_err_t
*/
esp_err_t hid_class_request_set_report(hid_host_device_handle_t hid_dev_handle,
                                       uint8_t report_type,
                                       uint8_t report_id,
                                       uint8_t *report,
                                       size_t report_length);

/**
 * @brief HID class specific request SET IDLE
 *
 * @param[in] hid_dev_handle    HID Device handle
 * @param[in] duration          0 (zero) for the indefinite duration, non-zero, then a fixed duration used.
 * @param[in] report_id         If 0 (zero) the idle rate applies to all input reports generated by the device, otherwise ReportID
 * @return esp_err_t
 */
esp_err_t hid_class_request_set_idle(hid_host_device_handle_t hid_dev_handle,
                                     uint8_t duration,
                                     uint8_t report_id);

/**
 * @brief HID class specific request SET PROTOCOL
 *
 * @param[in] hid_dev_handle    HID Device handle
 * @param[in] protocol          HID report protocol (boot or report)
 * @return esp_err_t
 */
esp_err_t hid_class_request_set_protocol(hid_host_device_handle_t hid_dev_handle,
        hid_report_protocol_t protocol);

#ifdef __cplusplus
}
#endif //__cplusplus
