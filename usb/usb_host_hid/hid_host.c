/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/param.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "usb/usb_host.h"

#include "usb/hid_host.h"

// HID spinlock
static portMUX_TYPE hid_lock = portMUX_INITIALIZER_UNLOCKED;
#define HID_ENTER_CRITICAL()    portENTER_CRITICAL(&hid_lock)
#define HID_EXIT_CRITICAL()     portEXIT_CRITICAL(&hid_lock)

// HID verification macroses
#define HID_GOTO_ON_FALSE_CRITICAL(exp, err)    \
    do {                                        \
        if(!(exp)) {                            \
            HID_EXIT_CRITICAL();                \
            ret = err;                          \
            goto fail;                          \
        }                                       \
    } while(0)

#define HID_RETURN_ON_FALSE_CRITICAL(exp, err)  \
    do {                                        \
        if(!(exp)) {                            \
            HID_EXIT_CRITICAL();                \
            return err;                         \
        }                                       \
    } while(0)

#define HID_GOTO_ON_ERROR(exp, msg) ESP_GOTO_ON_ERROR(exp, fail, TAG, msg)

#define HID_GOTO_ON_FALSE(exp, err, msg) ESP_GOTO_ON_FALSE( (exp), err, fail, TAG, msg )

#define HID_RETURN_ON_ERROR(exp, msg) ESP_RETURN_ON_ERROR((exp), TAG, msg)

#define HID_RETURN_ON_FALSE(exp, err, msg) ESP_RETURN_ON_FALSE( (exp), (err), TAG, msg)

#define HID_RETURN_ON_INVALID_ARG(exp) ESP_RETURN_ON_FALSE((exp) != NULL, ESP_ERR_INVALID_ARG, TAG, "Argument error")

static const char *TAG = "hid-host";

#define DEFAULT_TIMEOUT_MS  (5000)

typedef enum {
    HID_EP_IN = 0,
    HID_EP_OUT,
    HID_EP_MAX
} hid_ep_num_t;

typedef struct {
    SemaphoreHandle_t busy;                                 /**< HID device main mutex */
    SemaphoreHandle_t sem_done;                             /**< Control transfer semaphore */
    usb_transfer_t *xfer;                                   /**< Pointer to control transfer buffer */
} hid_host_ctrl_t;

/**
 * @brief HID Device object
 */
typedef struct hid_dev_obj {
    // Dynamic members require a critical section
    struct {
        STAILQ_ENTRY(hid_dev_obj) tailq_entry;                  /**< STAILQ entry of opened USB Deivce containing HID interfaces */
        STAILQ_HEAD(interface, iface_obj) iface_opened_tailq;   /**< STAILQ of HID interfaces, related to current USB device, containing the HID compatible Interface */
        union {
            struct {
                uint32_t claimed_interfaces: 8;                     /**< Claimed Interfaces */
                uint32_t reserved24: 24;                            /**< Reserved bit filed */
            };
            uint32_t val;
        } flags;
    } dynamic;
    // Constant members do no change after registration thus do not require a critical section
    struct {
        uint8_t usb_addr;                                           /**< USB Device address on bus */
        usb_device_handle_t dev_hdl;                                /**< USB Device handle */
        hid_host_ctrl_t ctrl;                                       /**< USB Device Control EP */
    } constant;
} hid_dev_obj_t;

/**
 * @brief HID Interface object
*/
typedef struct iface_obj {
    // Dynamic members require a critical section
    struct {
        STAILQ_ENTRY(iface_obj) tailq_entry;                  /**< STAILQ entry of HID interfaces, related to current USB device, containing the HID compatible Interface */
    } dynamic;
    struct {
        hid_dev_obj_t *hid_dev_obj_hdl;     /**< HID Device object handle */
        uint8_t num;                        /**< HID Interface Number */
        uint8_t sub_class;                  /**< HID Interface SubClass */
        uint8_t proto;                      /**< HID Interface Protocol */
        uint16_t report_descriptor_length;  /**< HID Report Descriptor Length */
        uint8_t country_code;               /**< HID Coutry Code */
        struct {
            uint8_t num;
            uint16_t mps;
            usb_transfer_t *xfer;                /**< Pointer to a transfer buffer */
        } ep[HID_EP_MAX];
    } constant;
} hid_iface_new_t;

ESP_EVENT_DEFINE_BASE(HID_HOST_EVENTS);

/**
 * @brief HID driver default context
 *
 * This context is created during HID Host install.
 */
typedef struct {
    STAILQ_HEAD(dev_obj, hid_dev_obj) dev_opened_tailq;         /**< STAILQ of opened USB Deivce containing HID interfaces */

    esp_event_loop_handle_t  event_loop_handle;
    usb_host_client_handle_t client_handle;                     /**< Client task handle */
    esp_event_handler_t user_cb;                                /**< User application callback */
    void *user_arg;                                             /**< User application callback args */
    bool event_handling_started;                                /**< Events handler started flag */
    SemaphoreHandle_t all_events_handled;                       /**< Events handler semaphore */
    volatile bool client_event_handling_started;                /**< Client event Start handling flag */
    volatile bool end_client_event_handling;                    /**< Client event End handling flag */
} hid_driver_t;

static hid_driver_t *s_hid_driver;                              /**< Internal pointer to HID driver */

// ----------------------- Private Prototypes ----------------------------------

// --------------------------- Internal Logic ----------------------------------
/**
 * @brief HID class specific request
*/
typedef struct hid_class_request {
    uint8_t bRequest;               /**< bRequest  */
    uint16_t wValue;                /**< wValue: Report Type and Report ID */
    uint16_t wIndex;                /**< wIndex: Interface */
    uint16_t wLength;               /**< wLength: Report Length */
    uint8_t *data;                  /**< Pointer to data */
} hid_class_request_t;

// ----------------- USB Event Handler - Internal Task -------------------------

/**
 * @brief USB Event handler
 *
 * Handle all USB related events such as USB host (usbh) events or hub events from USB hardware
 *
 * @param[in] arg   Argument, does not used
 */
static void event_handler_task(void *arg)
{
    ESP_LOGD(TAG, "USB HID handling start");
    while (hid_host_handle_events(portMAX_DELAY) == ESP_OK) {
    }
    ESP_LOGD(TAG, "USB HID handling stop");
    vTaskDelete(NULL);
}

static esp_err_t hid_host_device_get_opened_by_addr(uint8_t usb_addr,
        hid_dev_obj_t **hid_dev_obj_hdl)
{
    hid_dev_obj_t *dev_obj = NULL;

    HID_ENTER_CRITICAL();
    STAILQ_FOREACH(dev_obj, &s_hid_driver->dev_opened_tailq, dynamic.tailq_entry) {
        if (usb_addr == dev_obj->constant.usb_addr) {
            HID_EXIT_CRITICAL();
            *hid_dev_obj_hdl = dev_obj;
            return ESP_OK;
        }
    }
    HID_EXIT_CRITICAL();
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t hid_host_device_get_opened_by_handle(usb_device_handle_t dev_hdl,
        hid_dev_obj_t **hid_dev_obj_hdl)
{
    hid_dev_obj_t *dev_obj = NULL;

    HID_ENTER_CRITICAL();
    STAILQ_FOREACH(dev_obj, &s_hid_driver->dev_opened_tailq, dynamic.tailq_entry) {
        if (dev_hdl == dev_obj->constant.dev_hdl) {
            HID_EXIT_CRITICAL();
            *hid_dev_obj_hdl = dev_obj;
            return ESP_OK;
        }
    }
    HID_EXIT_CRITICAL();
    return ESP_ERR_NOT_FOUND;
}

static void hid_host_new_device_event(uint8_t dev_addr)
{
    usb_device_handle_t dev_hdl;
    const usb_config_desc_t *config_desc = NULL;
    const usb_intf_desc_t *iface_desc = NULL;
    int offset = 0;
    hid_host_event_data_t event_data = { 0 };
    size_t event_data_size = sizeof(hid_host_event_data_t);

    // open usb device
    if (usb_host_device_open(s_hid_driver->client_handle, dev_addr, &dev_hdl) == ESP_OK) {
        // get config descriptor
        if (usb_host_get_active_config_descriptor(dev_hdl, &config_desc) == ESP_OK) {
            // for every interface
            for (int num = 0; num < config_desc->bNumInterfaces; num++) {
                iface_desc = usb_parse_interface_descriptor(config_desc, num, 0, &offset);
                // if interface is HID compatible - notify user
                if (USB_CLASS_HID == iface_desc->bInterfaceClass) {

                    event_data.connect.usb.addr = dev_addr;
                    event_data.connect.usb.iface_num = iface_desc->bInterfaceNumber;
                    event_data.connect.usb.sub_class = iface_desc->bInterfaceSubClass;
                    event_data.connect.usb.proto = iface_desc->bInterfaceProtocol;

                    esp_event_post_to(s_hid_driver->event_loop_handle,
                                      HID_HOST_EVENTS,
                                      HID_HOST_CONNECT_EVENT,
                                      &event_data,
                                      event_data_size,
                                      portMAX_DELAY);
                }
            }
        }
    }
    // close device
    ESP_ERROR_CHECK(usb_host_device_close(s_hid_driver->client_handle, dev_hdl));
}

static void hid_host_device_notify_device_gone(hid_dev_obj_t *hid_dev_obj_hdl)
{
    hid_host_event_data_t event_data = { 0 };
    size_t event_data_size = sizeof(hid_host_event_data_t);
    hid_iface_new_t *hid_iface = NULL;

    HID_ENTER_CRITICAL();
    STAILQ_FOREACH(hid_iface, &hid_dev_obj_hdl->dynamic.iface_opened_tailq, dynamic.tailq_entry) {
        event_data.disconnect.dev = (hid_host_device_handle_t) hid_iface;
        event_data.disconnect.usb.addr = hid_dev_obj_hdl->constant.usb_addr;
        event_data.disconnect.usb.iface_num = hid_iface->constant.num;
        event_data.disconnect.usb.sub_class = hid_iface->constant.sub_class;
        event_data.disconnect.usb.proto = hid_iface->constant.proto;

        esp_event_post_to(s_hid_driver->event_loop_handle,
                          HID_HOST_EVENTS,
                          HID_HOST_DISCONNECT_EVENT,
                          &event_data,
                          event_data_size,
                          portMAX_DELAY);
    }
    HID_EXIT_CRITICAL();
}

static void hid_host_remove_device_event(usb_device_handle_t dev_hdl)
{
    hid_dev_obj_t *hid_dev_obj_hdl;
    if (ESP_OK == hid_host_device_get_opened_by_handle(dev_hdl, &hid_dev_obj_hdl)) {
        hid_host_device_notify_device_gone(hid_dev_obj_hdl);
    }
}

static esp_err_t hid_host_device_interface_is_claimed(hid_host_dev_params_t *dev_params)
{
    hid_iface_new_t *hid_iface = NULL;
    hid_dev_obj_t *hid_dev_obj = NULL;
    HID_ENTER_CRITICAL();
    STAILQ_FOREACH(hid_dev_obj, &s_hid_driver->dev_opened_tailq, dynamic.tailq_entry) {
        STAILQ_FOREACH(hid_iface, &hid_dev_obj->dynamic.iface_opened_tailq, dynamic.tailq_entry) {
            if ((dev_params->addr == hid_iface->constant.hid_dev_obj_hdl->constant.usb_addr)
                    && (dev_params->iface_num == hid_iface->constant.num)
                    && (dev_params->sub_class == hid_iface->constant.sub_class)
                    && (dev_params->proto == hid_iface->constant.proto)) {
                HID_EXIT_CRITICAL();
                return ESP_OK;
            }
        }
    }
    HID_EXIT_CRITICAL();
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief USB Host Client's event callback
 *
 * @param[in] event    Client event message
 * @param[in] arg      Argument, does not used
 */
static void client_event_cb(const usb_host_client_event_msg_t *event, void *arg)
{
    if (event->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        hid_host_new_device_event(event->new_dev.address);
    } else if (event->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        hid_host_remove_device_event(event->dev_gone.dev_hdl);
    }
}

/**
 * @brief HID IN Transfer complete callback
 *
 * @param[in] transfer  Pointer to transfer data structure
 */
static void in_xfer_done(usb_transfer_t *in_xfer)
{
    assert(in_xfer);
    hid_iface_new_t *hid_iface = (hid_iface_new_t *)in_xfer->context;

    switch (in_xfer->status) {
    case USB_TRANSFER_STATUS_COMPLETED:
        // Notify user
        // hid_host_user_interface_callback(iface, HID_HOST_INTERFACE_EVENT_INPUT_REPORT);
        hid_host_event_data_t event_data;
        size_t event_data_size = sizeof(hid_host_event_data_t);

        event_data.input.usb.addr = hid_iface->constant.hid_dev_obj_hdl->constant.usb_addr;
        event_data.input.usb.iface_num = hid_iface->constant.num;
        event_data.input.usb.sub_class = hid_iface->constant.sub_class;
        event_data.input.usb.proto = hid_iface->constant.proto;

        event_data.input.dev = (hid_host_device_handle_t) hid_iface;

        event_data.input.length = in_xfer->actual_num_bytes;
        memcpy(event_data.input.data, in_xfer->data_buffer, in_xfer->actual_num_bytes);

        esp_event_post_to(s_hid_driver->event_loop_handle,
                          HID_HOST_EVENTS,
                          HID_HOST_INPUT_EVENT,
                          &event_data,
                          event_data_size,
                          portMAX_DELAY);
        // Relaunch transfer
        usb_host_transfer_submit(in_xfer);
        return;
    case USB_TRANSFER_STATUS_NO_DEVICE:
    case USB_TRANSFER_STATUS_CANCELED:
        // User is notified about device disconnection from usb_event_cb
        // No need to do anything
        return;
    default:
        // Any other error
        break;
    }
    ESP_LOGE(TAG, "Transfer failed, IN EP 0x%02X, status %d", in_xfer->bEndpointAddress,
             in_xfer->status);
    // TODO: Notify user about transfer or any other error
}

/**
 * @brief HID IN Transfer complete callback
 *
 * @param[in] transfer  Pointer to transfer data structure
 */
static void out_xfer_done(usb_transfer_t *out_xfer)
{
    assert(out_xfer);
    switch (out_xfer->status) {
    case USB_TRANSFER_STATUS_COMPLETED:
        // Great, nothing to do here
        return;
    case USB_TRANSFER_STATUS_NO_DEVICE:
    case USB_TRANSFER_STATUS_CANCELED:
        // User is notified about device disconnection from usb_event_cb
        // No need to do anything
        return;
    default:
        // Any other error
        break;
    }
    ESP_LOGE(TAG, "Transfer failed, OUT EP 0x%02X, status %d", out_xfer->bEndpointAddress,
             out_xfer->status);
    // TODO: Notify user about transfer or any other error
    // hid_iface_new_t *hid_iface = (hid_iface_new_t *)out_xfer->context;
}

/** Lock HID device from other task
 *
 * @param[in] hid_device    // TODO:
 * @param[out] xfer    // TODO:
 * @param[in] timeout_ms    Timeout of trying to take the mutex
 * @return esp_err_t
 */
static inline esp_err_t hid_dev_obj_ctrl_claim(hid_host_ctrl_t *ctrl,
        uint32_t timeout_ms,
        usb_transfer_t **xfer)
{
    HID_RETURN_ON_INVALID_ARG(ctrl);
    HID_RETURN_ON_INVALID_ARG(ctrl->xfer);

    if (ESP_OK == xSemaphoreTake(ctrl->busy, pdMS_TO_TICKS(timeout_ms))) {
        *xfer = ctrl->xfer;
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

/** Unlock HID device from other task
 *
 * @param[in] hid_device    // TODO:
 * @param[in] timeout_ms    Timeout of trying to take the mutex
 * @return esp_err_t
 */
static inline void hid_dev_obj_ctrl_release(hid_host_ctrl_t *ctrl)
{
    xSemaphoreGive(ctrl->busy);
}

/**
 * @brief HID Control transfer complete callback
 *
 * @param[in] ctrl_xfer  Pointer to transfer data structure
 */
static void ctrl_xfer_done(usb_transfer_t *ctrl_xfer)
{
    assert(ctrl_xfer);
    hid_host_ctrl_t *ctrl = (hid_host_ctrl_t *)ctrl_xfer->context;
    // TODO: verify context xfer and xfer done
    // assert(ctrl->xfer == ctrl_xfer);
    xSemaphoreGive(ctrl->sem_done);
}

/**
 * @brief HID Host control transfer synchronous.
 *
 * @note  Passes interface and endpoint descriptors to obtain:

 *        - interface number, IN endpoint, OUT endpoint, max. packet size
 *
 * @param[in] hid_dev_obj_hdl  // TODO:
 * @param[in] len              // TODO: Number of bytes to transfer
 * @param[in] timeout_ms       Timeout in ms
 * @return esp_err_t
 */
static esp_err_t hid_dev_obj_ctrl_xfer(hid_host_ctrl_t *ctrl,
                                       usb_device_handle_t dev_hdl,
                                       size_t len,
                                       uint32_t timeout_ms)
{
    usb_transfer_t *xfer = ctrl->xfer;
    xfer->device_handle = dev_hdl;
    xfer->callback = ctrl_xfer_done;
    xfer->context = (void *) ctrl;
    xfer->bEndpointAddress = 0;
    xfer->timeout_ms = timeout_ms;
    xfer->num_bytes = len;

    HID_RETURN_ON_ERROR(usb_host_transfer_submit_control(s_hid_driver->client_handle,
                        xfer),
                        "Unable to submit control transfer");

    BaseType_t received = xSemaphoreTake(ctrl->sem_done,
                                         pdMS_TO_TICKS(xfer->timeout_ms));
    if (received != pdTRUE) {
        // Transfer was not finished, error in USB LIB. Reset the endpoint
        ESP_LOGE(TAG, "Control Transfer Timeout");

        HID_RETURN_ON_ERROR(usb_host_endpoint_halt(dev_hdl, xfer->bEndpointAddress),
                            "Unable to HALT EP");
        HID_RETURN_ON_ERROR(usb_host_endpoint_flush(dev_hdl, xfer->bEndpointAddress),
                            "Unable to FLUSH EP");
        usb_host_endpoint_clear(dev_hdl, xfer->bEndpointAddress);
        return ESP_ERR_TIMEOUT;
    }
    ESP_LOG_BUFFER_HEXDUMP(TAG, xfer->data_buffer, xfer->actual_num_bytes, ESP_LOG_DEBUG);
    return ESP_OK;
}

#if (0)
/**
 * @brief USB class standard request get descriptor
 *
 * @param[in] hid_dev_obj_hdl HID Device object handle
 * @param[in] req             Pointer to a class specific request structure
 * @return esp_err_t
 */
static esp_err_t usb_class_request_get_descriptor(hid_dev_obj_t *hid_dev_obj_hdl,
        const hid_class_request_t *req)
{
    esp_err_t ret;
    usb_transfer_t *xfer;
    HID_RETURN_ON_INVALID_ARG(req);
    HID_RETURN_ON_INVALID_ARG(req->data);

    HID_RETURN_ON_ERROR(hid_dev_obj_claim_ctrl_xfer(hid_dev_obj_hdl,
                        DEFAULT_TIMEOUT_MS,
                        &xfer),
                        "Control xfer buffer is busy");

    const size_t ctrl_size = xfer->data_buffer_size;

    if (ctrl_size < (USB_SETUP_PACKET_SIZE + req->wLength)) {
        usb_device_info_t dev_info;
        ESP_ERROR_CHECK(usb_host_device_info(hid_dev_obj_hdl->constant.dev_hdl, &dev_info));
        // reallocate the ctrl xfer buffer for new length
        ESP_LOGD(TAG, "Change HID ctrl xfer size from %d to %d",
                 ctrl_size,
                 (int)(USB_SETUP_PACKET_SIZE + req->wLength));

        usb_host_transfer_free(hid_dev_obj_hdl->constant.ctrl_xfer);
        HID_RETURN_ON_ERROR(usb_host_transfer_alloc(USB_SETUP_PACKET_SIZE + req->wLength,
                            0,
                            &hid_dev_obj_hdl->constant.ctrl_xfer),
                            "Unable to allocate transfer buffer for EP0");
    }

    usb_setup_packet_t *setup = (usb_setup_packet_t *)xfer->data_buffer;

    setup->bmRequestType = USB_BM_REQUEST_TYPE_DIR_IN |
                           USB_BM_REQUEST_TYPE_TYPE_STANDARD |
                           USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
    setup->bRequest = req->bRequest;
    setup->wValue = req->wValue;
    setup->wIndex = req->wIndex;
    setup->wLength = req->wLength;

    ret = hid_dev_obj_ctrl_xfer(hid_dev_obj_hdl, /* ->constant.dev_hdl, */
                                //    xfer,
                                //    (void *) hid_dev_obj_hdl,
                                USB_SETUP_PACKET_SIZE + req->wLength,
                                DEFAULT_TIMEOUT_MS);

    if (ESP_OK == ret) {
        xfer->actual_num_bytes -= USB_SETUP_PACKET_SIZE;
        if (xfer->actual_num_bytes <= req->wLength) {
            memcpy(req->data, xfer->data_buffer + USB_SETUP_PACKET_SIZE, xfer->actual_num_bytes);
        } else {
            ret = ESP_ERR_INVALID_SIZE;
        }
    }
    hid_dev_obj_release_ctrl_xfer(hid_dev_obj_hdl);

    return ret;
}

/**
 * @brief HID Host Request Report Descriptor
 *
 * @param[in] hidh_iface      // TODO:
 * @return esp_err_t
 */
static esp_err_t hid_host_class_request_report_descriptor(hid_dev_obj_t *hid_dev_obj_hdl)
{
    HID_RETURN_ON_INVALID_ARG(iface);

    iface->report_desc = malloc(iface->report_desc_size);
    HID_RETURN_ON_FALSE(iface->report_desc,
                        ESP_ERR_NO_MEM,
                        "Unable to allocate memory");

    const hid_class_request_t get_desc = {
        .bRequest = USB_B_REQUEST_GET_DESCRIPTOR,
        .wValue = (HID_CLASS_DESCRIPTOR_TYPE_REPORT << 8),
        .wIndex = iface->dev_params.iface_num,
        .wLength = iface->report_desc_size,
        .data = iface->report_desc
    };

    return usb_class_request_get_descriptor(iface->parent, &get_desc);
    return ESP_OK;
}
#endif //

/**
 * @brief HID class specific request Set
 *
 * @param[in] hid_device // TODO:
 * @param[in] req        Pointer to a class specific request structure
 * @return esp_err_t
 */
static esp_err_t hid_class_request_set(hid_dev_obj_t *hid_dev_obj_hdl,
                                       const hid_class_request_t *req)
{
    esp_err_t ret;
    usb_transfer_t *xfer;

    HID_RETURN_ON_ERROR(hid_dev_obj_ctrl_claim(&hid_dev_obj_hdl->constant.ctrl,
                        DEFAULT_TIMEOUT_MS,
                        &xfer),
                        "USB Device is busy by other task");

    usb_setup_packet_t *setup = (usb_setup_packet_t *)xfer->data_buffer;
    setup->bmRequestType = USB_BM_REQUEST_TYPE_DIR_OUT |
                           USB_BM_REQUEST_TYPE_TYPE_CLASS |
                           USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
    setup->bRequest = req->bRequest;
    setup->wValue = req->wValue;
    setup->wIndex = req->wIndex;
    setup->wLength = req->wLength;

    if (req->wLength && req->data) {
        memcpy(xfer->data_buffer + USB_SETUP_PACKET_SIZE, req->data, req->wLength);
    }

    ret = hid_dev_obj_ctrl_xfer(&hid_dev_obj_hdl->constant.ctrl,
                                hid_dev_obj_hdl->constant.dev_hdl,
                                USB_SETUP_PACKET_SIZE + setup->wLength,
                                DEFAULT_TIMEOUT_MS);

    hid_dev_obj_ctrl_release(&hid_dev_obj_hdl->constant.ctrl);
    return ret;
}

/**
 * @brief HID class specific request Get
 *
 * @param[in] hid_device    // TODO:
 * @param[in] req           Pointer to a class specific request structure
 * @param[out] out_length   Length of the response in data buffer of req struct
 * @return esp_err_t
 */
static esp_err_t hid_class_request_get(hid_dev_obj_t *hid_dev_obj_hdl,
                                       const hid_class_request_t *req,
                                       size_t *out_length)
{
    esp_err_t ret;
    usb_transfer_t *xfer;

    HID_RETURN_ON_ERROR(hid_dev_obj_ctrl_claim(&hid_dev_obj_hdl->constant.ctrl,
                        DEFAULT_TIMEOUT_MS,
                        &xfer),
                        "USB Device is busy by other task");

    usb_setup_packet_t *setup = (usb_setup_packet_t *)xfer->data_buffer;

    setup->bmRequestType = USB_BM_REQUEST_TYPE_DIR_IN |
                           USB_BM_REQUEST_TYPE_TYPE_CLASS |
                           USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
    setup->bRequest = req->bRequest;
    setup->wValue = req->wValue;
    setup->wIndex = req->wIndex;
    setup->wLength = req->wLength;

    ret = hid_dev_obj_ctrl_xfer(&hid_dev_obj_hdl->constant.ctrl,
                                hid_dev_obj_hdl->constant.dev_hdl,
                                USB_SETUP_PACKET_SIZE + setup->wLength,
                                DEFAULT_TIMEOUT_MS);

    if (ESP_OK == ret) {
        // We do not need the setup data, which is still in the transfer data buffer
        xfer->actual_num_bytes -= USB_SETUP_PACKET_SIZE;
        // Copy data if the size is ok
        if (xfer->actual_num_bytes <= req->wLength) {
            memcpy(req->data, xfer->data_buffer + USB_SETUP_PACKET_SIZE, xfer->actual_num_bytes);
            // return actual num bytes of response
            if (out_length) {
                *out_length = xfer->actual_num_bytes;
            }
        } else {
            ret = ESP_ERR_INVALID_SIZE;
        }
    }

    hid_dev_obj_ctrl_release(&hid_dev_obj_hdl->constant.ctrl);
    return ret;
}

// ---------------------------- Private ---------------------------------------
static esp_err_t hid_host_string_descriptor_copy(wchar_t *dest,
        const usb_str_desc_t *src)
{
    if (dest == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (src != NULL) {
        size_t len = MIN((src->bLength - USB_STANDARD_DESC_SIZE) / 2, HID_STR_DESC_MAX_LENGTH - 1);
        /*
        * To pass the warning during compilation, there is a
        * '-Wno-address-of-packed-member' flag added to this component
        */
        wcsncpy(dest, src->wData, len);
        // This should be always true, we just check to avoid LoadProhibited exception
        if (dest != NULL) {
            dest[len] = 0;
        }
    } else {
        dest[0] = 0;
    }
    return ESP_OK;
}

// ----------------------------- Public ----------------------------------------
static void hid_host_event_handler_wrapper(void *event_handler_arg,
        esp_event_base_t event_base,
        int32_t event_id,
        void *event_data)
{
    if (s_hid_driver->user_cb) {
        s_hid_driver->user_cb(event_handler_arg, event_base, event_id, event_data);
    }
}

esp_err_t hid_host_install(const hid_host_driver_config_t *config)
{
    esp_err_t ret;

    HID_RETURN_ON_INVALID_ARG(config);
    HID_RETURN_ON_INVALID_ARG(config->callback);

    if (config->create_background_task) {
        HID_RETURN_ON_FALSE(config->stack_size != 0,
                            ESP_ERR_INVALID_ARG,
                            "Wrong stack size value");
        HID_RETURN_ON_FALSE(config->task_priority != 0,
                            ESP_ERR_INVALID_ARG,
                            "Wrong task priority value");
    }

    HID_RETURN_ON_FALSE(!s_hid_driver,
                        ESP_ERR_INVALID_STATE,
                        "HID Host driver is already installed");

    // Create HID driver structure
    hid_driver_t *driver = heap_caps_calloc(1, sizeof(hid_driver_t), MALLOC_CAP_DEFAULT);
    HID_RETURN_ON_FALSE(driver,
                        ESP_ERR_NO_MEM,
                        "Unable to allocate memory");

    driver->user_cb = config->callback;
    driver->user_arg = config->callback_arg;

    usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .async.client_event_callback = client_event_cb,
        .async.callback_arg = NULL,
        .max_num_event_msg = 10,
    };

    driver->end_client_event_handling = false;
    driver->all_events_handled = xSemaphoreCreateBinary();
    HID_GOTO_ON_FALSE(driver->all_events_handled,
                      ESP_ERR_NO_MEM,
                      "Unable to create semaphore");

    HID_GOTO_ON_ERROR(usb_host_client_register(&client_config,
                      &driver->client_handle),
                      "Unable to register USB Host client");

    // create event loop
    esp_event_loop_args_t event_task_args = {
        .queue_size = 5,
        .task_name = "usb_hidd_events",
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = 4096,
        .task_core_id = tskNO_AFFINITY
    };
    HID_GOTO_ON_ERROR(esp_event_loop_create(&event_task_args,
                                            &driver->event_loop_handle),
                      "HID device event loop could not be created");

    HID_GOTO_ON_ERROR(esp_event_handler_register_with(driver->event_loop_handle,
                      HID_HOST_EVENTS,
                      HID_HOST_ANY_EVENT,
                      hid_host_event_handler_wrapper,
                      config->callback_arg),
                      "HID device event loop register handler failure");

    HID_ENTER_CRITICAL();
    HID_GOTO_ON_FALSE_CRITICAL(!s_hid_driver, ESP_ERR_INVALID_STATE);
    s_hid_driver = driver;
    STAILQ_INIT(&s_hid_driver->dev_opened_tailq);
    HID_EXIT_CRITICAL();

    if (config->create_background_task) {
        BaseType_t task_created = xTaskCreatePinnedToCore(
                                      event_handler_task,
                                      "USB HID Host",
                                      config->stack_size,
                                      NULL,
                                      config->task_priority,
                                      NULL,
                                      config->core_id);
        HID_GOTO_ON_FALSE(task_created,
                          ESP_ERR_NO_MEM,
                          "Unable to create USB HID Host task");
    }
    return ESP_OK;

fail:
    s_hid_driver = NULL;
    if (driver->event_loop_handle) {
        esp_event_loop_delete(driver->event_loop_handle);
    }
    if (driver->client_handle) {
        usb_host_client_deregister(driver->client_handle);
    }
    if (driver->all_events_handled) {
        vSemaphoreDelete(driver->all_events_handled);
    }
    free(driver);
    return ret;
}

esp_err_t hid_host_uninstall(void)
{
    // Make sure hid driver is installed,
    HID_RETURN_ON_FALSE(s_hid_driver,
                        ESP_OK,
                        "HID Host driver was not installed");

    // Make sure that hid driver not being uninstalled from other task
    // and no hid device is opened
    HID_ENTER_CRITICAL();
    HID_RETURN_ON_FALSE_CRITICAL(!s_hid_driver->end_client_event_handling, ESP_ERR_INVALID_STATE);
    HID_RETURN_ON_FALSE_CRITICAL(STAILQ_EMPTY(&s_hid_driver->dev_opened_tailq), ESP_ERR_INVALID_STATE);
    s_hid_driver->end_client_event_handling = true;
    HID_EXIT_CRITICAL();

    if (s_hid_driver->client_event_handling_started) {
        ESP_ERROR_CHECK(usb_host_client_unblock(s_hid_driver->client_handle));
        // In case the event handling started, we must wait until it finishes
        xSemaphoreTake(s_hid_driver->all_events_handled, portMAX_DELAY);
    }
    if (s_hid_driver->event_loop_handle) {
        esp_event_loop_delete(s_hid_driver->event_loop_handle);
    }
    vSemaphoreDelete(s_hid_driver->all_events_handled);
    ESP_ERROR_CHECK(usb_host_client_deregister(s_hid_driver->client_handle));
    free(s_hid_driver);
    s_hid_driver = NULL;
    return ESP_OK;
}

esp_err_t hid_host_device_add(uint8_t usb_addr,
                              hid_dev_obj_t **hid_dev_obj_hdl)
{
    usb_device_handle_t dev_hdl;
    // Open USB Device by USB addres on a bus
    esp_err_t ret = usb_host_device_open(s_hid_driver->client_handle,
                                         usb_addr,
                                         &dev_hdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to open USB Device with params");
        goto fail;
    }

    // Create an 'hid_dev_obj' object, which represent USB Device with HID compatible Interface
    hid_dev_obj_t *hid_dev_obj = calloc(1, sizeof(hid_dev_obj_t));

    if (NULL == hid_dev_obj) {
        ESP_LOGE(TAG, "Unable to allocate memory for HID Device");
        ret = ESP_ERR_NO_MEM;
        goto config_fail;
    }

    hid_dev_obj->constant.ctrl.busy = xSemaphoreCreateMutex();

    if (NULL == hid_dev_obj->constant.ctrl.busy) {
        ESP_LOGE(TAG, "Unable to create mutex for HID Device");
        ret = ESP_ERR_NO_MEM;
        goto mem_fail;
    }

    hid_dev_obj->constant.ctrl.sem_done =  xSemaphoreCreateBinary();

    if (NULL == hid_dev_obj->constant.ctrl.sem_done) {
        ESP_LOGE(TAG, "Unable to create semaphore for HID Device");
        ret = ESP_ERR_NO_MEM;
        goto mem_fail2;
    }

    /*
    * TIP: Usually, we need to allocate 'EP bMaxPacketSize0 + 1' here.
    * To take the size of a report descriptor into a consideration,
    * we need to allocate more here, e.g. 512 bytes.
    */
    ret = usb_host_transfer_alloc(512, 0, &hid_dev_obj->constant.ctrl.xfer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to allocate transfer buffer");
        goto mem_fail3;
    }

    hid_dev_obj->constant.usb_addr = usb_addr;
    hid_dev_obj->constant.dev_hdl = dev_hdl;

    HID_ENTER_CRITICAL();
    hid_dev_obj->dynamic.flags.val = 0;
    STAILQ_INIT(&hid_dev_obj->dynamic.iface_opened_tailq);
    STAILQ_INSERT_TAIL(&s_hid_driver->dev_opened_tailq, hid_dev_obj, dynamic.tailq_entry);
    HID_EXIT_CRITICAL();

    ESP_LOGD(TAG, "New Device has been added to the tQ (USB port %d)",
             hid_dev_obj->constant.usb_addr);

    *hid_dev_obj_hdl = hid_dev_obj;
    return ESP_OK;

mem_fail3:
    if (hid_dev_obj->constant.ctrl.sem_done) {
        vSemaphoreDelete(hid_dev_obj->constant.ctrl.sem_done);
    }
mem_fail2:
    if (hid_dev_obj->constant.ctrl.busy) {
        vSemaphoreDelete(hid_dev_obj->constant.ctrl.busy);
    }
mem_fail:
    free(hid_dev_obj);
config_fail:
    usb_host_device_close(s_hid_driver->client_handle, dev_hdl);
fail:
    return ret;
}

esp_err_t hid_host_device_remove(hid_dev_obj_t *hid_dev_obj_hdl)
{
    ESP_ERROR_CHECK(usb_host_transfer_free(hid_dev_obj_hdl->constant.ctrl.xfer));

    if (hid_dev_obj_hdl->constant.ctrl.sem_done) {
        vSemaphoreDelete(hid_dev_obj_hdl->constant.ctrl.sem_done);
    }
    if (hid_dev_obj_hdl->constant.ctrl.busy) {
        vSemaphoreDelete(hid_dev_obj_hdl->constant.ctrl.busy);
    }
    // 7. Close device in USB Host library
    ESP_ERROR_CHECK(usb_host_device_close(s_hid_driver->client_handle,
                                          hid_dev_obj_hdl->constant.dev_hdl));
    // 8. Remove hid_dev_obj from tQ
    HID_ENTER_CRITICAL();
    STAILQ_REMOVE(&s_hid_driver->dev_opened_tailq,
                  hid_dev_obj_hdl,
                  hid_dev_obj,
                  dynamic.tailq_entry);
    HID_EXIT_CRITICAL();

    ESP_LOGD(TAG, "Device has been removed from tQ (USB port %d)",
             hid_dev_obj_hdl->constant.usb_addr);

    free(hid_dev_obj_hdl);
    return ESP_OK;
}

esp_err_t hid_host_device_interface_claim(hid_dev_obj_t *hid_dev_obj_hdl,
        uint8_t iface_num,
        uint8_t sub_class,
        uint8_t proto)
{
    const usb_config_desc_t *config_desc = NULL;
    const usb_intf_desc_t *iface_desc = NULL;
    const hid_descriptor_t *hid_desc = NULL;
    const usb_ep_desc_t *ep_desc = NULL;
    int offset = 0;

    esp_err_t ret = usb_host_get_active_config_descriptor(hid_dev_obj_hdl->constant.dev_hdl,
                    &config_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to get Configuration Descriptor");
        return ret;
    }

    iface_desc = usb_parse_interface_descriptor(config_desc, iface_num, 0, &offset);
    if (NULL == iface_desc) {
        ESP_LOGE(TAG, "Unable to get Interface Descriptor");
        return ret;
    }

    // Create 'iface_obj' object
    hid_iface_new_t *hid_iface = calloc(1, sizeof(hid_iface_new_t));

    if (NULL == hid_iface) {
        ESP_LOGE(TAG, "Unable to allocate memory for HID Interface");
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }

    // Flush EP
    for (int i = 0; i < HID_EP_MAX; i++) {
        hid_iface->constant.ep[i].num = 0;
        hid_iface->constant.ep[i].mps = 0;
        hid_iface->constant.ep[i].xfer = NULL;
    }

    // HID descriptor
    size_t total_length = config_desc->wTotalLength;
    hid_desc = (const hid_descriptor_t *) usb_parse_next_descriptor_of_type((const usb_standard_desc_t *) iface_desc,
               total_length,
               HID_CLASS_DESCRIPTOR_TYPE_HID,
               &offset);
    if (!hid_desc) {
        ESP_LOGE(TAG, "Unable to find HID Descriptor");
        ret = ESP_ERR_NOT_FOUND;
        goto mem_fail;
    }

    // fill the constant fields
    hid_iface->constant.hid_dev_obj_hdl = hid_dev_obj_hdl;
    hid_iface->constant.num = iface_num;
    hid_iface->constant.sub_class = sub_class;
    hid_iface->constant.proto = proto;
    hid_iface->constant.report_descriptor_length = hid_desc->wReportDescriptorLength;
    hid_iface->constant.country_code = hid_desc->bCountryCode;
    // EP descriptors for Interface
    for (int i = 0; i < iface_desc->bNumEndpoints; i++) {
        int ep_offset = 0;
        ep_desc = usb_parse_endpoint_descriptor_by_index(iface_desc, i, total_length, &ep_offset);

        if (ep_desc) {
            if (USB_EP_DESC_GET_EP_DIR(ep_desc)) {
                hid_iface->constant.ep[HID_EP_IN].num = ep_desc->bEndpointAddress;
                hid_iface->constant.ep[HID_EP_IN].mps = ep_desc->wMaxPacketSize;
            } else {
                hid_iface->constant.ep[HID_EP_OUT].num = ep_desc->bEndpointAddress;
                hid_iface->constant.ep[HID_EP_OUT].mps = ep_desc->wMaxPacketSize;
            }
        } else {
            ESP_LOGE(TAG, "Unable to find HID Descriptor");
            ret = ESP_ERR_NOT_FOUND;
            goto mem_fail;
        }

    } // for every EP within Interface

    for (int i = 0; i < HID_EP_MAX; i++) {
        if (hid_iface->constant.ep[i].num) {
            ESP_LOGD(TAG, "HID Interface init EP %x", hid_iface->constant.ep[i].num);
            // create xfer for Endpoint
            ESP_ERROR_CHECK(usb_host_transfer_alloc(hid_iface->constant.ep[i].mps,
                                                    0,
                                                    &hid_iface->constant.ep[i].xfer));
        }
    }

    // 5. Claim Interface
    HID_RETURN_ON_ERROR(usb_host_interface_claim(s_hid_driver->client_handle,
                        hid_dev_obj_hdl->constant.dev_hdl,
                        hid_iface->constant.num, 0),
                        "Unable to claim Interface");

    // 6. Add Interface to the iface_opened queue
    HID_ENTER_CRITICAL();
    STAILQ_INSERT_TAIL(&hid_dev_obj_hdl->dynamic.iface_opened_tailq,
                       hid_iface,
                       dynamic.tailq_entry);
    hid_dev_obj_hdl->dynamic.flags.claimed_interfaces++;
    HID_EXIT_CRITICAL();

    ESP_LOGD(TAG, "HID Interface %d has been added to the tQ (USB port %d)",
             iface_num,
             hid_dev_obj_hdl->constant.usb_addr);

    // 7. Notify about opened Interface
    hid_host_event_data_t event_data = { 0 };
    size_t event_data_size = sizeof(hid_host_event_data_t);
    event_data.open.dev = (hid_host_device_handle_t) hid_iface;
    esp_event_post_to(s_hid_driver->event_loop_handle,
                      HID_HOST_EVENTS,
                      HID_HOST_OPEN_EVENT,
                      &event_data,
                      event_data_size,
                      portMAX_DELAY);

    return ESP_OK;

mem_fail:
    free(hid_iface);
fail:
    return ret;
}

esp_err_t hid_host_device_interface_release(hid_iface_new_t *hid_iface)
{
    hid_dev_obj_t *hid_dev_obj_hdl = hid_iface->constant.hid_dev_obj_hdl;
    // 1. Verify iface_obj in tQ
    // if (hid_host_interface_active(hid_iface)) {
    // 2. Stop EP IN transfer for the interface
    // HID_RETURN_ON_ERROR( usb_host_endpoint_halt(iface->parent->dev_hdl, iface->ep_in),
    //                      "Unable to HALT EP");
    // HID_RETURN_ON_ERROR( usb_host_endpoint_flush(iface->parent->dev_hdl, iface->ep_in),
    //                      "Unable to FLUSH EP");
    // usb_host_endpoint_clear(iface->parent->dev_hdl, iface->ep_in);
    // }

    // 3. Release interface
    HID_RETURN_ON_ERROR(usb_host_interface_release(s_hid_driver->client_handle,
                        hid_dev_obj_hdl->constant.dev_hdl,
                        hid_iface->constant.num),
                        "Unable to release HID Interface");

    // 4. Free all urb's and close pipe for EP IN
    for (int i = 0; i < HID_EP_MAX; i++) {
        if (hid_iface->constant.ep[i].num) {
            ESP_LOGD(TAG, "HID Interface deinit EP %x", hid_iface->constant.ep[i].num);
            ESP_ERROR_CHECK(usb_host_transfer_free(hid_iface->constant.ep[i].xfer));
        }
    }

    // 5. Delete iface_obj from tQ
    ESP_LOGD(TAG, "Remove HID Interface=%d from tQ (USB port %d)",
             hid_iface->constant.num,
             hid_dev_obj_hdl->constant.usb_addr);

    HID_ENTER_CRITICAL();
    STAILQ_REMOVE(&hid_dev_obj_hdl->dynamic.iface_opened_tailq,
                  hid_iface,
                  iface_obj,
                  dynamic.tailq_entry);
    hid_dev_obj_hdl->dynamic.flags.claimed_interfaces--;
    HID_EXIT_CRITICAL();

    free(hid_iface);
    return ESP_OK;
}

esp_err_t hid_host_device_open(hid_host_dev_params_t *dev_params)
{
    hid_dev_obj_t *hid_dev_obj_hdl = NULL;
    // 1. Search the USB device by addr in dev_opened queue
    if (ESP_ERR_NOT_FOUND == hid_host_device_get_opened_by_addr(dev_params->addr, &hid_dev_obj_hdl)) {
        // Open new USB device
        HID_RETURN_ON_ERROR(hid_host_device_add(dev_params->addr, &hid_dev_obj_hdl),
                            "Unable to open new USB Device");
    }
    // 2. Verify Interface has been already claimed
    if (ESP_OK == hid_host_device_interface_is_claimed(dev_params)) {
        ESP_LOGE(TAG, "HID Interface %d has been already claimed (USB Port %d)",
                 dev_params->iface_num,
                 dev_params->addr);
        return ESP_ERR_INVALID_STATE;
    }
    // 3. Claim Interface if possible
    HID_RETURN_ON_ERROR(hid_host_device_interface_claim(hid_dev_obj_hdl,
                        dev_params->iface_num,
                        dev_params->sub_class,
                        dev_params->proto),
                        "Unable to open new HID Interface");
    return ESP_OK;
}

esp_err_t hid_host_device_close(hid_host_device_handle_t hid_dev_handle)
{
    hid_iface_new_t *hid_iface = (hid_iface_new_t *)hid_dev_handle;
    hid_dev_obj_t *hid_dev_obj_hdl = hid_iface->constant.hid_dev_obj_hdl;

    HID_RETURN_ON_ERROR(hid_host_device_interface_release(hid_iface),
                        "HID Interface release failure");

    // Remove device if no Interfaces are claimed
    if (0 == hid_dev_obj_hdl->dynamic.flags.claimed_interfaces) {
        HID_RETURN_ON_ERROR(hid_host_device_remove(hid_dev_obj_hdl),
                            "USB HID Device close failure");
    }
    return ESP_OK;
}

esp_err_t hid_host_handle_events(uint32_t timeout)
{
    HID_RETURN_ON_FALSE(s_hid_driver != NULL,
                        ESP_ERR_INVALID_STATE,
                        "HID Driver is not installed");

    ESP_LOGD(TAG, "USB HID handle events");
    s_hid_driver->client_event_handling_started = true;
    esp_err_t ret = usb_host_client_handle_events(s_hid_driver->client_handle, timeout);
    if (s_hid_driver->end_client_event_handling) {
        xSemaphoreGive(s_hid_driver->all_events_handled);
        return ESP_FAIL;
    }
    return ret;
}

esp_err_t hid_host_get_device_info(hid_host_device_handle_t hid_dev_handle,
                                   hid_host_dev_info_t *hid_dev_info)
{
    HID_RETURN_ON_INVALID_ARG(hid_dev_info);

    hid_iface_new_t *hid_iface = (hid_iface_new_t *)hid_dev_handle;
    hid_dev_obj_t *hid_dev_obj_hdl = hid_iface->constant.hid_dev_obj_hdl;

    // Fill descriptor device information
    const usb_device_desc_t *desc;
    usb_device_info_t dev_info;

    HID_RETURN_ON_ERROR(usb_host_get_device_descriptor(hid_dev_obj_hdl->constant.dev_hdl, &desc),
                        "Unable to get device descriptor");
    HID_RETURN_ON_ERROR(usb_host_device_info(hid_dev_obj_hdl->constant.dev_hdl, &dev_info),
                        "Unable to get USB device info");
    // VID, PID
    hid_dev_info->VID = desc->idVendor;
    hid_dev_info->PID = desc->idProduct;

    // Strings
    hid_host_string_descriptor_copy(hid_dev_info->iManufacturer,
                                    dev_info.str_desc_manufacturer);
    hid_host_string_descriptor_copy(hid_dev_info->iProduct,
                                    dev_info.str_desc_product);
    hid_host_string_descriptor_copy(hid_dev_info->iSerialNumber,
                                    dev_info.str_desc_serial_num);

    // HID related info
    hid_dev_info->bInterfaceNum = hid_iface->constant.num;
    hid_dev_info->bSubClass = hid_iface->constant.sub_class;
    hid_dev_info->bProtocol = hid_iface->constant.proto;
    hid_dev_info->wReportDescriptorLenght = hid_iface->constant.report_descriptor_length;
    hid_dev_info->bCountryCode = hid_iface->constant.country_code;

    return ESP_OK;
}

// ------------------------ USB HID Host driver API ----------------------------
esp_err_t hid_host_device_enable_input(hid_host_device_handle_t hid_dev_handle)
{
    hid_iface_new_t *hid_iface = (hid_iface_new_t *)hid_dev_handle;
    hid_dev_obj_t *hid_dev_obj_hdl = hid_iface->constant.hid_dev_obj_hdl;
    usb_transfer_t *xfer = hid_iface->constant.ep[HID_EP_IN].xfer;
    // prepare transfer
    xfer->device_handle = hid_dev_obj_hdl->constant.dev_hdl;
    xfer->callback = in_xfer_done;
    xfer->context = hid_iface;
    xfer->timeout_ms = DEFAULT_TIMEOUT_MS;
    xfer->bEndpointAddress = hid_iface->constant.ep[HID_EP_IN].num;
    xfer->num_bytes = hid_iface->constant.ep[HID_EP_IN].mps;
    return usb_host_transfer_submit(xfer);
}

esp_err_t hid_host_device_disable_input(hid_host_device_handle_t hid_dev_handle)
{
    hid_iface_new_t *hid_iface = (hid_iface_new_t *)hid_dev_handle;
    hid_dev_obj_t *hid_dev_obj_hdl = hid_iface->constant.hid_dev_obj_hdl;
    HID_RETURN_ON_ERROR(usb_host_endpoint_halt(hid_dev_obj_hdl->constant.dev_hdl,
                        hid_iface->constant.ep[HID_EP_IN].num),
                        "Unable to HALT EP");
    HID_RETURN_ON_ERROR(usb_host_endpoint_flush(hid_dev_obj_hdl->constant.dev_hdl,
                        hid_iface->constant.ep[HID_EP_IN].num),
                        "Unable to FLUSH EP");
    usb_host_endpoint_clear(hid_dev_obj_hdl->constant.dev_hdl,
                            hid_iface->constant.ep[HID_EP_IN].num);
    return ESP_OK;
}

esp_err_t hid_host_device_output(hid_host_device_handle_t hid_dev_handle,
                                 const uint8_t *data,
                                 const size_t length)
{
    hid_iface_new_t *hid_iface = (hid_iface_new_t *)hid_dev_handle;
    hid_dev_obj_t *hid_dev_obj_hdl = hid_iface->constant.hid_dev_obj_hdl;
    usb_transfer_t *xfer = hid_iface->constant.ep[HID_EP_OUT].xfer;

    if (xfer == NULL) {
        ESP_LOGE(TAG, "USB HID Device doesn't have OUT EP");
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (length > hid_iface->constant.ep[HID_EP_OUT].mps) {
        // TODO: we can easily send the data by chunks
        // so, lets leave it to the next version
        ESP_LOGE(TAG, "Data length overflow OUT EP mps");
        return ESP_ERR_INVALID_SIZE;
    }

    // Prepare transfer
    xfer->device_handle = hid_dev_obj_hdl->constant.dev_hdl;
    xfer->callback = out_xfer_done;
    xfer->context = hid_iface;
    xfer->timeout_ms = DEFAULT_TIMEOUT_MS;
    xfer->bEndpointAddress = hid_iface->constant.ep[HID_EP_OUT].num;
    xfer->num_bytes = length;
    memcpy(xfer->data_buffer, data, length);
    return usb_host_transfer_submit(xfer);
}

uint8_t *hid_host_get_report_descriptor(hid_host_device_handle_t hid_dev_handle,
                                        size_t *report_desc_len)
{
#if (0)
    hid_iface_t *iface = get_iface_by_handle(hid_dev_handle);

    if (NULL == iface) {
        return NULL;
    }

    // Report Descriptor was already requested, return pointer
    if (iface->report_desc) {
        *report_desc_len = iface->report_desc_size;
        return iface->report_desc;
    }

    // Request Report Descriptor
    if (ESP_OK == hid_class_request_report_descriptor(iface)) {
        *report_desc_len = iface->report_desc_size;
        return iface->report_desc;
    }
#endif //
    return NULL;
}

esp_err_t hid_class_request_get_report(hid_host_device_handle_t hid_dev_handle,
                                       uint8_t report_type,
                                       uint8_t report_id,
                                       uint8_t *report,
                                       size_t *report_length)
{
    hid_iface_new_t *hid_iface = (hid_iface_new_t *)hid_dev_handle;

    const hid_class_request_t get_report = {
        .bRequest = HID_CLASS_SPECIFIC_REQ_GET_REPORT,
        .wValue = (report_type << 8) | report_id,
        .wIndex = hid_iface->constant.num,
        .wLength = *report_length,
        .data = report
    };

    return hid_class_request_get(hid_iface->constant.hid_dev_obj_hdl, &get_report, report_length);
}

esp_err_t hid_class_request_get_idle(hid_host_device_handle_t hid_dev_handle,
                                     uint8_t report_id,
                                     uint8_t *idle_rate)
{
    hid_iface_new_t *hid_iface = (hid_iface_new_t *)hid_dev_handle;

    uint8_t tmp[1] = { 0xff };

    const hid_class_request_t get_idle = {
        .bRequest = HID_CLASS_SPECIFIC_REQ_GET_IDLE,
        .wValue = report_id,
        .wIndex = hid_iface->constant.num,
        .wLength = 1,
        .data = tmp
    };

    HID_RETURN_ON_ERROR(hid_class_request_get(hid_iface->constant.hid_dev_obj_hdl,
                        &get_idle,
                        NULL),
                        "HID class request transfer failure");

    *idle_rate = tmp[0];
    return ESP_OK;
}

esp_err_t hid_class_request_get_protocol(hid_host_device_handle_t hid_dev_handle,
        hid_report_protocol_t *protocol)
{
    hid_iface_new_t *hid_iface = (hid_iface_new_t *)hid_dev_handle;

    uint8_t tmp[1] = { 0xff };

    const hid_class_request_t get_proto = {
        .bRequest = HID_CLASS_SPECIFIC_REQ_GET_PROTOCOL,
        .wValue = 0,
        .wIndex = hid_iface->constant.num,
        .wLength = 1,
        .data = tmp
    };

    HID_RETURN_ON_ERROR(hid_class_request_get(hid_iface->constant.hid_dev_obj_hdl,
                        &get_proto,
                        NULL),
                        "HID class request failure");

    *protocol = (hid_report_protocol_t) tmp[0];
    return ESP_OK;
}

esp_err_t hid_class_request_set_report(hid_host_device_handle_t hid_dev_handle,
                                       uint8_t report_type,
                                       uint8_t report_id,
                                       uint8_t *report,
                                       size_t report_length)
{
    hid_iface_new_t *hid_iface = (hid_iface_new_t *)hid_dev_handle;

    const hid_class_request_t set_report = {
        .bRequest = HID_CLASS_SPECIFIC_REQ_SET_REPORT,
        .wValue = (report_type << 8) | report_id,
        .wIndex = hid_iface->constant.num,
        .wLength = report_length,
        .data = report
    };

    return hid_class_request_set(hid_iface->constant.hid_dev_obj_hdl, &set_report);
}

esp_err_t hid_class_request_set_idle(hid_host_device_handle_t hid_dev_handle,
                                     uint8_t duration,
                                     uint8_t report_id)
{
    hid_iface_new_t *hid_iface = (hid_iface_new_t *)hid_dev_handle;

    const hid_class_request_t set_idle = {
        .bRequest = HID_CLASS_SPECIFIC_REQ_SET_IDLE,
        .wValue = (duration << 8) | report_id,
        .wIndex = hid_iface->constant.num,
        .wLength = 0,
        .data = NULL
    };

    return hid_class_request_set(hid_iface->constant.hid_dev_obj_hdl, &set_idle);
}

esp_err_t hid_class_request_set_protocol(hid_host_device_handle_t hid_dev_handle,
        hid_report_protocol_t protocol)
{
    hid_iface_new_t *hid_iface = (hid_iface_new_t *)hid_dev_handle;

    const hid_class_request_t set_proto = {
        .bRequest = HID_CLASS_SPECIFIC_REQ_SET_PROTOCOL,
        .wValue = protocol,
        .wIndex = hid_iface->constant.num,
        .wLength = 0,
        .data = NULL
    };

    return hid_class_request_set(hid_iface->constant.hid_dev_obj_hdl, &set_proto);
}
