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

/**
 * @brief HID Device structure.
 *
 */
typedef struct hid_host_device {
    STAILQ_ENTRY(hid_host_device) tailq_entry;  /**< HID device queue */
    SemaphoreHandle_t device_busy;              /**< HID device main mutex */
    SemaphoreHandle_t ctrl_xfer_done;           /**< Control transfer semaphore */
    usb_transfer_t *ctrl_xfer;                  /**< Pointer to control transfer buffer */
    usb_device_handle_t dev_hdl;                /**< USB device handle */
    uint8_t dev_addr;                           /**< USB devce address */
} hid_device_t;

/**
 * @brief HID Interface state
*/
typedef enum {
    HID_INTERFACE_STATE_NOT_INITIALIZED = 0x00, /**< HID Interface not initialized */
    HID_INTERFACE_STATE_IDLE,                   /**< HID Interface has been found in connected USB device */
    HID_INTERFACE_STATE_READY,                  /**< HID Interface opened and ready to start transfer */
    HID_INTERFACE_STATE_ACTIVE,                 /**< HID Interface is in use */
    HID_INTERFACE_STATE_WAIT_USER_DELETION,     /**< HID Interface wait user to be removed */
    HID_INTERFACE_STATE_MAX
} hid_iface_state_t;

/**
 * @brief HID Interface structure in device to interact with. After HID device opening keeps the interface configuration
 *
 */
typedef struct hid_interface {
    STAILQ_ENTRY(hid_interface) tailq_entry;
    hid_device_t *parent;                   /**< Parent USB HID device */
    hid_host_dev_params_t dev_params;       /**< USB device parameters */
    uint8_t ep_in;                          /**< Interrupt IN EP number */
    uint16_t ep_in_mps;                     /**< Interrupt IN max size */
    uint8_t country_code;                   /**< Country code */
    uint16_t report_desc_size;              /**< Size of Report */
    uint8_t *report_desc;                   /**< Pointer to HID Report */
    usb_transfer_t *in_xfer;                /**< Pointer to IN transfer buffer */
    hid_host_interface_event_cb_t user_cb;  /**< Interface application callback */
    void *user_cb_arg;                      /**< Interface application callback arg */
    hid_iface_state_t state;                /**< Interface state */
} hid_iface_t;

/**
 * @brief HID driver default context
 *
 * This context is created during HID Host install.
 */
typedef struct {
    STAILQ_HEAD(devices, hid_host_device) hid_devices_tailq;    /**< STAILQ of HID interfaces */
    STAILQ_HEAD(interfaces, hid_interface) hid_ifaces_tailq;    /**< STAILQ of HID interfaces */
    usb_host_client_handle_t client_handle;                     /**< Client task handle */
    hid_host_driver_event_cb_t user_cb;                         /**< User application callback */
    void *user_arg;                                             /**< User application callback args */
    bool event_handling_started;                                /**< Events handler started flag */
    SemaphoreHandle_t all_events_handled;                       /**< Events handler semaphore */
    volatile bool end_client_event_handling;                    /**< Client event handling flag */
} hid_driver_t;

static hid_driver_t *s_hid_driver;                              /**< Internal pointer to HID driver */


// ----------------------- Private Prototypes ----------------------------------

static esp_err_t hid_host_install_device(uint8_t dev_addr,
        usb_device_handle_t dev_hdl,
        hid_device_t **hid_device);


static esp_err_t hid_host_uninstall_device(hid_device_t *hid_device);

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

/**
 * @brief Return HID device in devices list by USB device handle
 *
 * @param[in] usb_device_handle_t   USB device handle
 * @return hid_device_t Pointer to device, NULL if device not present
 */
static hid_device_t *get_hid_device_by_handle(usb_device_handle_t usb_handle)
{
    hid_device_t *device = NULL;

    HID_ENTER_CRITICAL();
    STAILQ_FOREACH(device, &s_hid_driver->hid_devices_tailq, tailq_entry) {
        if (usb_handle == device->dev_hdl) {
            HID_EXIT_CRITICAL();
            return device;
        }
    }
    HID_EXIT_CRITICAL();
    return NULL;
}

/**
 * @brief Return HID Device fron the transfer context
 *
 * @param[in] xfer   USB transfer struct
 * @return hid_device_t Pointer to HID Device
 */
static inline hid_device_t *get_hid_device_from_context(usb_transfer_t *xfer)
{
    return (hid_device_t *)xfer->context;
}

/**
 * @brief Get HID Interface pointer by Endpoint address
 *
 * @param[in] ep_addr      Endpoint address
 * @return hid_iface_t     Pointer to HID Interface configuration structure
 */
static hid_iface_t *get_interface_by_ep(uint8_t ep_addr)
{
    hid_iface_t *interface = NULL;

    HID_ENTER_CRITICAL();
    STAILQ_FOREACH(interface, &s_hid_driver->hid_ifaces_tailq, tailq_entry) {
        if (ep_addr == interface->ep_in) {
            HID_EXIT_CRITICAL();
            return interface;
        }
    }

    HID_EXIT_CRITICAL();
    return NULL;
}

/**
 * @brief Verify presence of Interface in the RAM list
 *
 * @param[in] iface         Pointer to an Interface structure
 * @return true             Interface is in the list
 * @return false            Interface is not in the list
 */
static inline bool is_interface_in_list(hid_iface_t *iface)
{
    hid_iface_t *interface = NULL;

    HID_ENTER_CRITICAL();
    STAILQ_FOREACH(interface, &s_hid_driver->hid_ifaces_tailq, tailq_entry) {
        if (iface == interface) {
            HID_EXIT_CRITICAL();
            return true;
        }
    }

    HID_EXIT_CRITICAL();
    return false;
}

/**
 * @brief Get HID Interface pointer by external HID Device handle with verification in RAM list
 *
 * @param[in] hid_dev_handle HID Device handle
 * @return hid_iface_t       Pointer to an Interface structure
 */
static hid_iface_t *get_iface_by_handle(hid_host_device_handle_t hid_dev_handle)
{
    hid_iface_t *hid_iface = (hid_iface_t *) hid_dev_handle;

    if (!is_interface_in_list(hid_iface)) {
        ESP_LOGE(TAG, "HID interface handle not found");
        return NULL;
    }

    return hid_iface;
}

/**
 * @brief Check HID interface descriptor present
 *
 * @param[in] config_desc  Pointer to Configuration Descriptor
 * @return esp_err_t
 */
static bool hid_interface_present(const usb_config_desc_t *config_desc)
{
    const usb_intf_desc_t *iface_desc = NULL;
    int offset = 0;

    for (int num = 0; num < config_desc->bNumInterfaces; num++) {
        iface_desc = usb_parse_interface_descriptor(config_desc, num, 0, &offset);
        if (USB_CLASS_HID == iface_desc->bInterfaceClass) {
            return true;
        }
    }
    return false;
}

/**
 * @brief HID Interface user callback function.
 *
 * @param[in] hid_iface   Pointer to an Interface structure
 * @param[in] event_id    HID Interface event
 */
static inline void hid_host_user_interface_callback(hid_iface_t *hid_iface,
        const hid_host_interface_event_t event)
{
    assert(hid_iface);

    hid_host_dev_params_t *dev_params = &hid_iface->dev_params;

    assert(dev_params);

    if (hid_iface->user_cb) {
        hid_iface->user_cb(hid_iface, event, hid_iface->user_cb_arg);
    }
}

/**
 * @brief HID Device user callback function.
 *
 * @param[in] event_id    HID Device event
 * @param[in] dev_params  HID Device parameters
 */
static inline void hid_host_user_device_callback(hid_iface_t *hid_iface,
        const hid_host_driver_event_t event)
{
    assert(hid_iface);

    hid_host_dev_params_t *dev_params = &hid_iface->dev_params;

    assert(dev_params);

    if (s_hid_driver && s_hid_driver->user_cb) {
        s_hid_driver->user_cb(hid_iface, event, s_hid_driver->user_arg);
    }
}

/**
 * @brief Add interface in a list
 *
 * @param[in] hid_device    HID device handle
 * @param[in] iface_desc  Pointer to an Interface descriptor
 * @param[in] hid_desc    Pointer to an HID device descriptor
 * @param[in] ep_desc     Pointer to an EP descriptor
 * @return esp_err_t
 */
static esp_err_t hid_host_add_interface(hid_device_t *hid_device,
                                        const usb_intf_desc_t *iface_desc,
                                        const hid_descriptor_t *hid_desc,
                                        const usb_ep_desc_t *ep_in_desc)
{
    hid_iface_t *hid_iface = calloc(1, sizeof(hid_iface_t));

    HID_RETURN_ON_FALSE(hid_iface,
                        ESP_ERR_NO_MEM,
                        "Unable to allocate memory");

    HID_ENTER_CRITICAL();
    hid_iface->parent = hid_device;
    hid_iface->state = HID_INTERFACE_STATE_NOT_INITIALIZED;
    hid_iface->dev_params.addr = hid_device->dev_addr;

    if (iface_desc) {
        hid_iface->dev_params.iface_num = iface_desc->bInterfaceNumber;
        hid_iface->dev_params.sub_class = iface_desc->bInterfaceSubClass;
        hid_iface->dev_params.proto = iface_desc->bInterfaceProtocol;
    }

    if (hid_desc) {
        hid_iface->country_code = hid_desc->bCountryCode;
        hid_iface->report_desc_size = hid_desc->wReportDescriptorLength;
    }

    // EP IN && INT Type
    if (ep_in_desc) {
        if ( (ep_in_desc->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) &&
                (ep_in_desc->bmAttributes & USB_B_ENDPOINT_ADDRESS_EP_NUM_MASK) ) {
            hid_iface->ep_in = ep_in_desc->bEndpointAddress;
            hid_iface->ep_in_mps = USB_EP_DESC_GET_MPS(ep_in_desc);
        } else {
            ESP_EARLY_LOGE(TAG, "HID device EP IN %#X configuration error",
                           ep_in_desc->bEndpointAddress);
        }
    }

    if (iface_desc && hid_desc && ep_in_desc) {
        hid_iface->state = HID_INTERFACE_STATE_IDLE;
    }

    STAILQ_INSERT_TAIL(&s_hid_driver->hid_ifaces_tailq, hid_iface, tailq_entry);
    HID_EXIT_CRITICAL();

    return ESP_OK;
}

/**
 * @brief Remove interface from a list
 *
 * Use only inside critical section
 *
 * @param[in] hid_iface    HID interface handle
 * @return esp_err_t
 */
static esp_err_t _hid_host_remove_interface(hid_iface_t *hid_iface)
{
    hid_iface->state = HID_INTERFACE_STATE_NOT_INITIALIZED;
    STAILQ_REMOVE(&s_hid_driver->hid_ifaces_tailq, hid_iface, hid_interface, tailq_entry);
    free(hid_iface);
    return ESP_OK;
}

/**
 * @brief Notify user about the connected Interfaces
 *
 * @param[in] hid_device  Pointer to HID device structure
 */
static void hid_host_notify_interface_connected(hid_device_t *hid_device)
{
    HID_ENTER_CRITICAL();
    hid_iface_t *iface = STAILQ_FIRST(&s_hid_driver->hid_ifaces_tailq);
    hid_iface_t *tmp = NULL;

    while (iface != NULL) {
        tmp = STAILQ_NEXT(iface, tailq_entry);
        HID_EXIT_CRITICAL();

        if (iface->parent && (iface->parent->dev_addr == hid_device->dev_addr)) {
            hid_host_user_device_callback(iface, HID_HOST_DRIVER_EVENT_CONNECTED);
        }
        iface = tmp;

        HID_ENTER_CRITICAL();
    }
    HID_EXIT_CRITICAL();
}

/**
 * @brief Create a list of available interfaces in RAM
 *
 * @param[in] hid_device  Pointer to HID device structure
 * @param[in] dev_addr    USB device physical address
 * @param[in] sub_class   USB HID SubClass value
 * @return esp_err_t
 */
static esp_err_t hid_host_interface_list_create(hid_device_t *hid_device,
        const usb_config_desc_t *config_desc)
{
    assert(hid_device);
    assert(config_desc);
    size_t total_length = config_desc->wTotalLength;
    const usb_intf_desc_t *iface_desc = NULL;
    const hid_descriptor_t *hid_desc = NULL;
    const usb_ep_desc_t *ep_desc = NULL;
    const usb_ep_desc_t *ep_in_desc = NULL;
    int offset = 0;

    // For every Interface
    iface_desc = (const usb_intf_desc_t *)usb_parse_next_descriptor_of_type((const usb_standard_desc_t *)config_desc,
                 config_desc->wTotalLength,
                 USB_B_DESCRIPTOR_TYPE_INTERFACE,
                 &offset);

    while (iface_desc != NULL) {
        if (USB_CLASS_HID == iface_desc->bInterfaceClass) {
            // HID descriptor
            ESP_LOGI(TAG, "Found HID, bInterfaceNumber=%d, offset=%d",
                     iface_desc->bInterfaceNumber,
                     offset);
            // Get HID descriptor and parse Endpoints
            hid_desc = (const hid_descriptor_t *) usb_parse_next_descriptor_of_type((const usb_standard_desc_t *) iface_desc,
                       total_length,
                       HID_CLASS_DESCRIPTOR_TYPE_HID,
                       &offset);
            if (hid_desc) {
                ep_in_desc = NULL;
                // EP descriptors for Interface
                for (int i = 0; i < iface_desc->bNumEndpoints; i++) {
                    int ep_offset = 0;
                    ep_desc = usb_parse_endpoint_descriptor_by_index(iface_desc, i, total_length, &ep_offset);
                    if (ep_desc) {
                        if (USB_EP_DESC_GET_EP_DIR(ep_desc)) {
                            ep_in_desc = ep_desc;
                        }
                    }
                } // for every EP within Interface

                if (ep_in_desc) {
                    // Add Interface to the list
                    HID_RETURN_ON_ERROR( hid_host_add_interface(hid_device,
                                         iface_desc,
                                         hid_desc,
                                         ep_in_desc),
                                         "Unable to add HID Interface to the RAM list");
                }
            }
        } // HID Interface
        iface_desc = (const usb_intf_desc_t *)usb_parse_next_descriptor_of_type((const usb_standard_desc_t *)iface_desc,
                     config_desc->wTotalLength,
                     USB_B_DESCRIPTOR_TYPE_INTERFACE,
                     &offset);
    }

    hid_host_notify_interface_connected(hid_device);

    return ESP_OK;
}

/**
 * @brief HID Host initialize device attempt
 *
 * @param[in] dev_addr   USB device physical address
 * @return true USB device contain HID Interface and device was initialized
 * @return false USB does not contain HID Interface
 */
static bool hid_host_device_init_attempt(uint8_t dev_addr)
{
    bool is_hid_device = false;
    usb_device_handle_t dev_hdl;
    const usb_config_desc_t *config_desc = NULL;
    hid_device_t *hid_device = NULL;

    if (usb_host_device_open(s_hid_driver->client_handle, dev_addr, &dev_hdl) == ESP_OK) {
        if (usb_host_get_active_config_descriptor(dev_hdl, &config_desc) == ESP_OK) {
            is_hid_device = hid_interface_present(config_desc);
        }
    }

    // Create HID interfaces list in RAM, connected to the particular USB dev
    if (is_hid_device) {
        // Proceed, add HID device to the list, get handle if necessary
        ESP_ERROR_CHECK( hid_host_install_device(dev_addr, dev_hdl, &hid_device) );
        // Create Interfaces list for a possibility to claim Interface
        ESP_ERROR_CHECK( hid_host_interface_list_create(hid_device, config_desc) );
    } else {
        usb_host_device_close(s_hid_driver->client_handle, dev_hdl);
        ESP_LOGW(TAG, "No HID device at USB port %d", dev_addr);
    }

    return is_hid_device;
}

/**
 * @brief USB device was removed we need to shutdown HID Interface
 *
 * @param[in] hid_dev_handle    Handle of the HID devive to close
 * @return esp_err_t
 */
static esp_err_t hid_host_interface_shutdown(hid_host_device_handle_t hid_dev_handle)
{
    hid_iface_t *hid_iface = get_iface_by_handle(hid_dev_handle);

    HID_RETURN_ON_INVALID_ARG(hid_iface);

    if (hid_iface->user_cb) {
        // Let user handle the remove process
        hid_iface->state = HID_INTERFACE_STATE_WAIT_USER_DELETION;
        hid_host_user_interface_callback(hid_iface, HID_HOST_INTERFACE_EVENT_DISCONNECTED);
    } else {
        // Remove HID Interface from the list right now
        ESP_LOGD(TAG, "Remove addr %d, iface %d from list",
                 hid_iface->dev_params.addr,
                 hid_iface->dev_params.iface_num);
        HID_ENTER_CRITICAL();
        _hid_host_remove_interface(hid_iface);
        HID_EXIT_CRITICAL();
    }

    return ESP_OK;
}

/**
 * @brief Deinit USB device by handle
 *
 * @param[in] dev_hdl   USB device handle
 * @return esp_err_t
 */
static esp_err_t hid_host_device_disconnected(usb_device_handle_t dev_hdl)
{
    hid_device_t *hid_device = get_hid_device_by_handle(dev_hdl);
    hid_iface_t *hid_iface = NULL;
    // Device should be in the list
    assert(hid_device);

    HID_ENTER_CRITICAL();
    while (!STAILQ_EMPTY(&s_hid_driver->hid_ifaces_tailq)) {
        hid_iface = STAILQ_FIRST(&s_hid_driver->hid_ifaces_tailq);
        HID_EXIT_CRITICAL();
        if (hid_iface->parent && (hid_iface->parent->dev_addr == hid_device->dev_addr)) {
            HID_RETURN_ON_ERROR( hid_host_device_close(hid_iface),
                                 "Unable to close device");
            HID_RETURN_ON_ERROR( hid_host_interface_shutdown(hid_iface),
                                 "Unable to shutdown interface");
        }
        HID_ENTER_CRITICAL();
    }
    HID_EXIT_CRITICAL();
    // Delete HID compliant device
    HID_RETURN_ON_ERROR( hid_host_uninstall_device(hid_device),
                         "Unable to uninstall device");

    return ESP_OK;
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
        hid_host_device_init_attempt(event->new_dev.address);
    } else if (event->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        hid_host_device_disconnected(event->dev_gone.dev_hdl);
    }
}

/**
 * @brief HID Host claim Interface and prepare transfer, change state to READY
 *
 * @param[in] iface       Pointer to Interface structure,
 * @return esp_err_t
 */
static esp_err_t hid_host_interface_claim_and_prepare_transfer(hid_iface_t *iface)
{
    HID_RETURN_ON_ERROR( usb_host_interface_claim( s_hid_driver->client_handle,
                         iface->parent->dev_hdl,
                         iface->dev_params.iface_num, 0),
                         "Unable to claim Interface");

    HID_RETURN_ON_ERROR( usb_host_transfer_alloc(iface->ep_in_mps, 0, &iface->in_xfer),
                         "Unable to allocate transfer buffer for EP IN");

    // Change state
    iface->state = HID_INTERFACE_STATE_READY;
    return ESP_OK;
}

/**
 * @brief HID Host release Interface and free transfer, change state to IDLE
 *
 * @param[in] iface       Pointer to Interface structure,
 * @return esp_err_t
 */
static esp_err_t hid_host_interface_release_and_free_transfer(hid_iface_t *iface)
{
    HID_RETURN_ON_INVALID_ARG(iface);
    HID_RETURN_ON_INVALID_ARG(iface->parent);

    HID_RETURN_ON_FALSE(is_interface_in_list(iface),
                        ESP_ERR_NOT_FOUND,
                        "Interface handle not found");

    HID_RETURN_ON_ERROR( usb_host_interface_release(s_hid_driver->client_handle,
                         iface->parent->dev_hdl,
                         iface->dev_params.iface_num),
                         "Unable to release HID Interface");

    ESP_ERROR_CHECK( usb_host_transfer_free(iface->in_xfer) );

    // Change state
    iface->state = HID_INTERFACE_STATE_IDLE;
    return ESP_OK;
}

/**
 * @brief Disable active interface
 *
 * @param[in] iface       Pointer to Interface structure
 * @return esp_err_t
 */
static esp_err_t hid_host_disable_interface(hid_iface_t *iface)
{
    HID_RETURN_ON_INVALID_ARG(iface);
    HID_RETURN_ON_INVALID_ARG(iface->parent);

    HID_RETURN_ON_FALSE(is_interface_in_list(iface),
                        ESP_ERR_NOT_FOUND,
                        "Interface handle not found");

    HID_RETURN_ON_FALSE((HID_INTERFACE_STATE_ACTIVE == iface->state),
                        ESP_ERR_INVALID_STATE,
                        "Interface wrong state");

    HID_RETURN_ON_ERROR( usb_host_endpoint_halt(iface->parent->dev_hdl, iface->ep_in),
                         "Unable to HALT EP");
    HID_RETURN_ON_ERROR( usb_host_endpoint_flush(iface->parent->dev_hdl, iface->ep_in),
                         "Unable to FLUSH EP");
    usb_host_endpoint_clear(iface->parent->dev_hdl, iface->ep_in);

    iface->state = HID_INTERFACE_STATE_READY;

    return ESP_OK;
}

/**
 * @brief HID IN Transfer complete callback
 *
 * @param[in] transfer  Pointer to transfer data structure
 */
static void in_xfer_done(usb_transfer_t *in_xfer)
{
    assert(in_xfer);

    hid_iface_t *iface = get_interface_by_ep(in_xfer->bEndpointAddress);
    assert(iface);

    // Interfaces' parent device should be the same as the hid_device in context
    assert(get_hid_device_from_context(in_xfer) == iface->parent);

    switch (in_xfer->status) {
    case USB_TRANSFER_STATUS_COMPLETED:
        // Notify user
        hid_host_user_interface_callback(iface, HID_HOST_INTERFACE_EVENT_INPUT_REPORT);
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

    ESP_LOGE(TAG, "Transfer failed, status %d", in_xfer->status);
    // Notify user about transfer or any other error
    hid_host_user_interface_callback(iface, HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR);
}

/** Lock HID device from other task
 *
 * @param[in] hid_device    Pointer to HID device structure
 * @param[in] timeout_ms    Timeout of trying to take the mutex
 * @return esp_err_t
 */
static inline esp_err_t hid_device_try_lock(hid_device_t *hid_device, uint32_t timeout_ms)
{
    return ( xSemaphoreTake(hid_device->device_busy, pdMS_TO_TICKS(timeout_ms))
             ? ESP_OK
             : ESP_ERR_TIMEOUT );
}

/** Unlock HID device from other task
 *
 * @param[in] hid_device    Pointer to HID device structure
 * @param[in] timeout_ms    Timeout of trying to take the mutex
 * @return esp_err_t
 */
static inline void hid_device_unlock(hid_device_t *hid_device)
{
    xSemaphoreGive(hid_device->device_busy);
}

/**
 * @brief HID Control transfer complete callback
 *
 * @param[in] ctrl_xfer  Pointer to transfer data structure
 */
static void ctrl_xfer_done(usb_transfer_t *ctrl_xfer)
{
    assert(ctrl_xfer);
    hid_device_t *hid_device = (hid_device_t *)ctrl_xfer->context;
    xSemaphoreGive(hid_device->ctrl_xfer_done);
}

/**
 * @brief HID control transfer synchronous.
 *
 * @note  Passes interface and endpoint descriptors to obtain:

 *        - interface number, IN endpoint, OUT endpoint, max. packet size
 *
 * @param[in] hid_device  Pointer to HID device structure
 * @param[in] ctrl_xfer   Pointer to the Transfer structure
 * @param[in] len         Number of bytes to transfer
 * @param[in] timeout_ms  Timeout in ms
 * @return esp_err_t
 */
static esp_err_t hid_control_transfer(hid_device_t *hid_device,
                                      size_t len,
                                      uint32_t timeout_ms)
{

    usb_transfer_t *ctrl_xfer = hid_device->ctrl_xfer;

    ctrl_xfer->device_handle = hid_device->dev_hdl;
    ctrl_xfer->callback = ctrl_xfer_done;
    ctrl_xfer->context = hid_device;
    ctrl_xfer->bEndpointAddress = 0;
    ctrl_xfer->timeout_ms = timeout_ms;
    ctrl_xfer->num_bytes = len;

    HID_RETURN_ON_ERROR( usb_host_transfer_submit_control(s_hid_driver->client_handle, ctrl_xfer),
                         "Unable to submit control transfer");

    BaseType_t received = xSemaphoreTake(hid_device->ctrl_xfer_done, pdMS_TO_TICKS(ctrl_xfer->timeout_ms));

    if (received != pdTRUE) {
        // Transfer was not finished, error in USB LIB. Reset the endpoint
        ESP_LOGE(TAG, "Control Transfer Timeout");

        HID_RETURN_ON_ERROR( usb_host_endpoint_halt(hid_device->dev_hdl, ctrl_xfer->bEndpointAddress),
                             "Unable to HALT EP");
        HID_RETURN_ON_ERROR( usb_host_endpoint_flush(hid_device->dev_hdl, ctrl_xfer->bEndpointAddress),
                             "Unable to FLUSH EP");
        usb_host_endpoint_clear(hid_device->dev_hdl, ctrl_xfer->bEndpointAddress);
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOG_BUFFER_HEXDUMP(TAG, ctrl_xfer->data_buffer, ctrl_xfer->actual_num_bytes, ESP_LOG_DEBUG);

    return ESP_OK;
}

/**
 * @brief USB class standard request get descriptor
 *
 * @param[in] hidh_device Pointer to HID device structure
 * @param[in] req         Pointer to a class specific request structure
 * @return esp_err_t
 */
static esp_err_t usb_class_request_get_descriptor(hid_device_t *hid_device, const hid_class_request_t *req)
{
    esp_err_t ret;
    usb_transfer_t *ctrl_xfer = hid_device->ctrl_xfer;
    const size_t ctrl_size = hid_device->ctrl_xfer->data_buffer_size;

    HID_RETURN_ON_INVALID_ARG(hid_device);
    HID_RETURN_ON_INVALID_ARG(hid_device->ctrl_xfer);
    HID_RETURN_ON_INVALID_ARG(req);
    HID_RETURN_ON_INVALID_ARG(req->data);

    HID_RETURN_ON_ERROR( hid_device_try_lock(hid_device, DEFAULT_TIMEOUT_MS),
                         "HID Device is busy by other task");

    if (ctrl_size < (USB_SETUP_PACKET_SIZE + req->wLength)) {
        usb_device_info_t dev_info;
        ESP_ERROR_CHECK(usb_host_device_info(hid_device->dev_hdl, &dev_info));
        // reallocate the ctrl xfer buffer for new length
        ESP_LOGD(TAG, "Change HID ctrl xfer size from %d to %d",
                 ctrl_size,
                 (int) (USB_SETUP_PACKET_SIZE + req->wLength));

        usb_host_transfer_free(hid_device->ctrl_xfer);
        HID_RETURN_ON_ERROR( usb_host_transfer_alloc(USB_SETUP_PACKET_SIZE + req->wLength,
                             0,
                             &hid_device->ctrl_xfer),
                             "Unable to allocate transfer buffer for EP0");
    }

    usb_setup_packet_t *setup = (usb_setup_packet_t *)ctrl_xfer->data_buffer;

    setup->bmRequestType = USB_BM_REQUEST_TYPE_DIR_IN |
                           USB_BM_REQUEST_TYPE_TYPE_STANDARD |
                           USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
    setup->bRequest = req->bRequest;
    setup->wValue = req->wValue;
    setup->wIndex = req->wIndex;
    setup->wLength = req->wLength;

    ret = hid_control_transfer(hid_device,
                               USB_SETUP_PACKET_SIZE + req->wLength,
                               DEFAULT_TIMEOUT_MS);

    if (ESP_OK == ret) {
        ctrl_xfer->actual_num_bytes -= USB_SETUP_PACKET_SIZE;
        if (ctrl_xfer->actual_num_bytes <= req->wLength) {
            memcpy(req->data, ctrl_xfer->data_buffer + USB_SETUP_PACKET_SIZE, ctrl_xfer->actual_num_bytes);
        } else {
            ret = ESP_ERR_INVALID_SIZE;
        }
    }

    hid_device_unlock(hid_device);

    return ret;
}

/**
 * @brief HID Host Request Report Descriptor
 *
 * @param[in] hidh_iface      Pointer to HID Interface configuration structure
 * @return esp_err_t
 */
static esp_err_t hid_class_request_report_descriptor(hid_iface_t *iface)
{
    HID_RETURN_ON_INVALID_ARG(iface);

    // Get Report Descritpor is possible only in Ready or Active state
    HID_RETURN_ON_FALSE((HID_INTERFACE_STATE_READY == iface->state) ||
                        (HID_INTERFACE_STATE_ACTIVE == iface->state),
                        ESP_ERR_INVALID_STATE,
                        "Unable to request report decriptor. Interface is not ready");

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
}

/**
 * @brief HID class specific request Set
 *
 * @param[in] hid_device Pointer to HID device structure
 * @param[in] req        Pointer to a class specific request structure
 * @return esp_err_t
 */
static esp_err_t hid_class_request_set(hid_device_t *hid_device,
                                       const hid_class_request_t *req)
{
    esp_err_t ret;
    usb_transfer_t *ctrl_xfer = hid_device->ctrl_xfer;
    HID_RETURN_ON_INVALID_ARG(hid_device);
    HID_RETURN_ON_INVALID_ARG(hid_device->ctrl_xfer);

    HID_RETURN_ON_ERROR( hid_device_try_lock(hid_device, DEFAULT_TIMEOUT_MS),
                         "HID Device is busy by other task");

    usb_setup_packet_t *setup = (usb_setup_packet_t *)ctrl_xfer->data_buffer;
    setup->bmRequestType = USB_BM_REQUEST_TYPE_DIR_OUT |
                           USB_BM_REQUEST_TYPE_TYPE_CLASS |
                           USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
    setup->bRequest = req->bRequest;
    setup->wValue = req->wValue;
    setup->wIndex = req->wIndex;
    setup->wLength = req->wLength;

    if (req->wLength && req->data) {
        memcpy(ctrl_xfer->data_buffer + USB_SETUP_PACKET_SIZE, req->data, req->wLength);
    }

    ret = hid_control_transfer(hid_device,
                               USB_SETUP_PACKET_SIZE + setup->wLength,
                               DEFAULT_TIMEOUT_MS);

    hid_device_unlock(hid_device);

    return ret;
}

/**
 * @brief HID class specific request Get
 *
 * @param[in] hid_device    Pointer to HID device structure
 * @param[in] req           Pointer to a class specific request structure
 * @param[out] out_length   Length of the response in data buffer of req struct
 * @return esp_err_t
 */
static esp_err_t hid_class_request_get(hid_device_t *hid_device,
                                       const hid_class_request_t *req,
                                       size_t *out_length)
{
    esp_err_t ret;
    HID_RETURN_ON_INVALID_ARG(hid_device);
    HID_RETURN_ON_INVALID_ARG(hid_device->ctrl_xfer);

    usb_transfer_t *ctrl_xfer = hid_device->ctrl_xfer;

    HID_RETURN_ON_ERROR( hid_device_try_lock(hid_device, DEFAULT_TIMEOUT_MS),
                         "HID Device is busy by other task");

    usb_setup_packet_t *setup = (usb_setup_packet_t *)ctrl_xfer->data_buffer;

    setup->bmRequestType = USB_BM_REQUEST_TYPE_DIR_IN |
                           USB_BM_REQUEST_TYPE_TYPE_CLASS |
                           USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
    setup->bRequest = req->bRequest;
    setup->wValue = req->wValue;
    setup->wIndex = req->wIndex;
    setup->wLength = req->wLength;

    ret = hid_control_transfer(hid_device,
                               USB_SETUP_PACKET_SIZE + setup->wLength,
                               DEFAULT_TIMEOUT_MS);

    if (ESP_OK == ret) {
        // We do not need the setup data, which is still in the transfer data buffer
        ctrl_xfer->actual_num_bytes -= USB_SETUP_PACKET_SIZE;
        // Copy data if the size is ok
        if (ctrl_xfer->actual_num_bytes <= req->wLength) {
            memcpy(req->data, ctrl_xfer->data_buffer + USB_SETUP_PACKET_SIZE, ctrl_xfer->actual_num_bytes);
            // return actual num bytes of response
            if (out_length) {
                *out_length = ctrl_xfer->actual_num_bytes;
            }
        } else {
            ret = ESP_ERR_INVALID_SIZE;
        }
    }

    hid_device_unlock(hid_device);

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

esp_err_t hid_host_install_device(uint8_t dev_addr,
                                  usb_device_handle_t dev_hdl,
                                  hid_device_t **hid_device_handle)
{
    esp_err_t ret;
    hid_device_t *hid_device;

    HID_GOTO_ON_FALSE( hid_device = calloc(1, sizeof(hid_device_t)),
                       ESP_ERR_NO_MEM,
                       "Unable to allocate memory for HID Device");

    hid_device->dev_addr = dev_addr;
    hid_device->dev_hdl = dev_hdl;

    HID_GOTO_ON_FALSE( hid_device->ctrl_xfer_done = xSemaphoreCreateBinary(),
                       ESP_ERR_NO_MEM,
                       "Unable to create semaphore");
    HID_GOTO_ON_FALSE( hid_device->device_busy =  xSemaphoreCreateMutex(),
                       ESP_ERR_NO_MEM,
                       "Unable to create semaphore");

    /*
    * TIP: Usually, we need to allocate 'EP bMaxPacketSize0 + 1' here.
    * To take the size of a report descriptor into a consideration,
    * we need to allocate more here, e.g. 512 bytes.
    */
    HID_GOTO_ON_ERROR(usb_host_transfer_alloc(512, 0, &hid_device->ctrl_xfer),
                      "Unable to allocate transfer buffer");

    HID_ENTER_CRITICAL();
    HID_GOTO_ON_FALSE_CRITICAL( s_hid_driver, ESP_ERR_INVALID_STATE );
    HID_GOTO_ON_FALSE_CRITICAL( s_hid_driver->client_handle, ESP_ERR_INVALID_STATE );
    STAILQ_INSERT_TAIL(&s_hid_driver->hid_devices_tailq, hid_device, tailq_entry);
    HID_EXIT_CRITICAL();

    if (hid_device_handle) {
        *hid_device_handle = hid_device;
    }

    return ESP_OK;

fail:
    hid_host_uninstall_device(hid_device);
    return ret;
}

esp_err_t hid_host_uninstall_device(hid_device_t *hid_device)
{
    HID_RETURN_ON_INVALID_ARG(hid_device);

    HID_RETURN_ON_ERROR( usb_host_transfer_free(hid_device->ctrl_xfer),
                         "Unablet to free transfer buffer for EP0");
    HID_RETURN_ON_ERROR( usb_host_device_close(s_hid_driver->client_handle,
                         hid_device->dev_hdl),
                         "Unable to close USB host");

    if (hid_device->ctrl_xfer_done) {
        vSemaphoreDelete(hid_device->ctrl_xfer_done);
    }

    if (hid_device->device_busy) {
        vSemaphoreDelete(hid_device->device_busy);
    }

    ESP_LOGD(TAG, "Remove addr %d device from list",
             hid_device->dev_addr);

    HID_ENTER_CRITICAL();
    STAILQ_REMOVE(&s_hid_driver->hid_devices_tailq, hid_device, hid_host_device, tailq_entry);
    HID_EXIT_CRITICAL();

    free(hid_device);
    return ESP_OK;
}

// ----------------------------- Public ----------------------------------------

esp_err_t hid_host_install(const hid_host_driver_config_t *config)
{
    esp_err_t ret;

    HID_RETURN_ON_INVALID_ARG(config);
    HID_RETURN_ON_INVALID_ARG(config->callback);

    if ( config->create_background_task ) {
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

    HID_GOTO_ON_ERROR( usb_host_client_register(&client_config,
                       &driver->client_handle),
                       "Unable to register USB Host client");

    HID_ENTER_CRITICAL();
    HID_GOTO_ON_FALSE_CRITICAL(!s_hid_driver, ESP_ERR_INVALID_STATE);
    s_hid_driver = driver;
    STAILQ_INIT(&s_hid_driver->hid_devices_tailq);
    STAILQ_INIT(&s_hid_driver->hid_ifaces_tailq);
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

    // Make sure that hid driver
    // not being uninstalled from other task
    // and no hid device is registered
    HID_ENTER_CRITICAL();
    HID_RETURN_ON_FALSE_CRITICAL( !s_hid_driver->end_client_event_handling, ESP_ERR_INVALID_STATE );
    HID_RETURN_ON_FALSE_CRITICAL( STAILQ_EMPTY(&s_hid_driver->hid_devices_tailq), ESP_ERR_INVALID_STATE );
    HID_RETURN_ON_FALSE_CRITICAL( STAILQ_EMPTY(&s_hid_driver->hid_ifaces_tailq), ESP_ERR_INVALID_STATE );
    s_hid_driver->end_client_event_handling = true;
    HID_EXIT_CRITICAL();

    if (s_hid_driver->event_handling_started) {
        ESP_ERROR_CHECK( usb_host_client_unblock(s_hid_driver->client_handle) );
        // In case the event handling started, we must wait until it finishes
        xSemaphoreTake(s_hid_driver->all_events_handled, portMAX_DELAY);
    }
    vSemaphoreDelete(s_hid_driver->all_events_handled);
    ESP_ERROR_CHECK( usb_host_client_deregister(s_hid_driver->client_handle) );
    free(s_hid_driver);
    s_hid_driver = NULL;
    return ESP_OK;
}

esp_err_t hid_host_device_open(hid_host_device_handle_t hid_dev_handle,
                               const hid_host_device_config_t *config)
{
    HID_RETURN_ON_FALSE(s_hid_driver,
                        ESP_ERR_INVALID_STATE,
                        "HID Driver is not installed");

    hid_iface_t *hid_iface = get_iface_by_handle(hid_dev_handle);

    HID_RETURN_ON_INVALID_ARG(hid_iface);

    HID_RETURN_ON_FALSE((hid_iface->dev_params.proto >= HID_PROTOCOL_NONE)
                        && (hid_iface->dev_params.proto < HID_PROTOCOL_MAX),
                        ESP_ERR_INVALID_ARG,
                        "HID device protocol not supported");

    HID_RETURN_ON_FALSE((HID_INTERFACE_STATE_IDLE == hid_iface->state),
                        ESP_ERR_INVALID_STATE,
                        "Interface wrong state");

    // Claim interface, allocate xfer and save report callback
    HID_RETURN_ON_ERROR( hid_host_interface_claim_and_prepare_transfer(hid_iface),
                         "Unable to claim interface");

    // Save HID Interface callback
    hid_iface->user_cb = config->callback;
    hid_iface->user_cb_arg = config->callback_arg;

    return ESP_OK;
}

esp_err_t hid_host_device_close(hid_host_device_handle_t hid_dev_handle)
{
    hid_iface_t *hid_iface = get_iface_by_handle(hid_dev_handle);

    HID_RETURN_ON_INVALID_ARG(hid_iface);

    ESP_LOGD(TAG, "Close addr %d, iface %d, state %d",
             hid_iface->dev_params.addr,
             hid_iface->dev_params.iface_num,
             hid_iface->state);

    if (HID_INTERFACE_STATE_ACTIVE == hid_iface->state) {
        HID_RETURN_ON_ERROR( hid_host_disable_interface(hid_iface),
                             "Unable to disable HID Interface");
    }

    if (HID_INTERFACE_STATE_READY == hid_iface->state) {
        HID_RETURN_ON_ERROR( hid_host_interface_release_and_free_transfer(hid_iface),
                             "Unable to release HID Interface");

        // If the device is closing by user before device detached we need to flush user callback here
        free(hid_iface->report_desc);
        hid_iface->report_desc = NULL;
    }

    if (HID_INTERFACE_STATE_WAIT_USER_DELETION == hid_iface->state) {
        hid_iface->user_cb = NULL;
        hid_iface->user_cb_arg = NULL;

        /* Remove Interface from the list */
        ESP_LOGD(TAG, "User Remove addr %d, iface %d from list",
                 hid_iface->dev_params.addr,
                 hid_iface->dev_params.iface_num);
        HID_ENTER_CRITICAL();
        _hid_host_remove_interface(hid_iface);
        HID_EXIT_CRITICAL();
    }

    return ESP_OK;
}

esp_err_t hid_host_handle_events(uint32_t timeout)
{
    HID_RETURN_ON_FALSE(s_hid_driver != NULL,
                        ESP_ERR_INVALID_STATE,
                        "HID Driver is not installed");

    ESP_LOGD(TAG, "USB HID handling");
    s_hid_driver->event_handling_started = true;
    esp_err_t ret = usb_host_client_handle_events(s_hid_driver->client_handle, timeout);
    if (s_hid_driver->end_client_event_handling) {
        xSemaphoreGive(s_hid_driver->all_events_handled);
        return ESP_FAIL;
    }
    return ret;
}

esp_err_t hid_host_device_get_params(hid_host_device_handle_t hid_dev_handle,
                                     hid_host_dev_params_t *dev_params)
{
    hid_iface_t *iface = get_iface_by_handle(hid_dev_handle);

    HID_RETURN_ON_FALSE(iface,
                        ESP_ERR_INVALID_STATE,
                        "HID Interface not found");

    HID_RETURN_ON_FALSE(dev_params,
                        ESP_ERR_INVALID_ARG,
                        "Wrong argument");

    memcpy(dev_params, &iface->dev_params, sizeof(hid_host_dev_params_t));
    return ESP_OK;
}

esp_err_t hid_host_device_get_raw_input_report_data(hid_host_device_handle_t hid_dev_handle,
        uint8_t *data,
        size_t data_length_max,
        size_t *data_length)
{
    hid_iface_t *iface = get_iface_by_handle(hid_dev_handle);

    HID_RETURN_ON_FALSE(iface,
                        ESP_ERR_INVALID_STATE,
                        "HID Interface not found");

    HID_RETURN_ON_FALSE(data,
                        ESP_ERR_INVALID_ARG,
                        "Wrong argument");

    HID_RETURN_ON_FALSE(data_length,
                        ESP_ERR_INVALID_ARG,
                        "Wrong argument");

    size_t copied = (data_length_max >= iface->in_xfer->actual_num_bytes)
                    ? iface->in_xfer->actual_num_bytes
                    : data_length_max;
    memcpy(data, iface->in_xfer->data_buffer, copied);
    *data_length = copied;
    return ESP_OK;
}

// ------------------------ USB HID Host driver API ----------------------------

esp_err_t hid_host_device_start(hid_host_device_handle_t hid_dev_handle)
{
    hid_iface_t *iface = get_iface_by_handle(hid_dev_handle);

    HID_RETURN_ON_INVALID_ARG(iface);
    HID_RETURN_ON_INVALID_ARG(iface->in_xfer);
    HID_RETURN_ON_INVALID_ARG(iface->parent);

    HID_RETURN_ON_FALSE(is_interface_in_list(iface),
                        ESP_ERR_NOT_FOUND,
                        "Interface handle not found");

    HID_RETURN_ON_FALSE ((HID_INTERFACE_STATE_READY == iface->state),
                         ESP_ERR_INVALID_STATE,
                         "Interface wrong state");

    // prepare transfer
    iface->in_xfer->device_handle = iface->parent->dev_hdl;
    iface->in_xfer->callback = in_xfer_done;
    iface->in_xfer->context = iface->parent;
    iface->in_xfer->timeout_ms = DEFAULT_TIMEOUT_MS;
    iface->in_xfer->bEndpointAddress = iface->ep_in;
    iface->in_xfer->num_bytes = iface->ep_in_mps;

    iface->state = HID_INTERFACE_STATE_ACTIVE;

    // start data transfer
    return usb_host_transfer_submit(iface->in_xfer);
}

esp_err_t hid_host_device_stop(hid_host_device_handle_t hid_dev_handle)
{
    hid_iface_t *iface = get_iface_by_handle(hid_dev_handle);

    HID_RETURN_ON_INVALID_ARG(iface);

    return hid_host_disable_interface(iface);
}

uint8_t *hid_host_get_report_descriptor(hid_host_device_handle_t hid_dev_handle,
                                        size_t *report_desc_len)
{
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

    return NULL;
}

esp_err_t hid_host_get_device_info(hid_host_device_handle_t hid_dev_handle,
                                   hid_host_dev_info_t *hid_dev_info)
{
    HID_RETURN_ON_INVALID_ARG(hid_dev_info);

    hid_iface_t *iface = get_iface_by_handle(hid_dev_handle);
    HID_RETURN_ON_INVALID_ARG(iface);

    hid_device_t *hid_dev = iface->parent;

    // Fill descriptor device information
    const usb_device_desc_t *desc;
    usb_device_info_t dev_info;
    HID_RETURN_ON_ERROR( usb_host_get_device_descriptor(hid_dev->dev_hdl, &desc),
                         "Unable to get device descriptor");
    HID_RETURN_ON_ERROR( usb_host_device_info(hid_dev->dev_hdl, &dev_info),
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
    return ESP_OK;
}

esp_err_t hid_class_request_get_report(hid_host_device_handle_t hid_dev_handle,
                                       uint8_t report_type,
                                       uint8_t report_id,
                                       uint8_t *report,
                                       size_t *report_length)
{
    hid_iface_t *iface = get_iface_by_handle(hid_dev_handle);

    HID_RETURN_ON_INVALID_ARG(iface);
    HID_RETURN_ON_INVALID_ARG(report);

    const hid_class_request_t get_report = {
        .bRequest = HID_CLASS_SPECIFIC_REQ_GET_REPORT,
        .wValue = (report_type << 8) | report_id,
        .wIndex = iface->dev_params.iface_num,
        .wLength = *report_length,
        .data = report
    };

    return hid_class_request_get(iface->parent, &get_report, report_length);
}

esp_err_t hid_class_request_get_idle(hid_host_device_handle_t hid_dev_handle,
                                     uint8_t report_id,
                                     uint8_t *idle_rate)
{
    hid_iface_t *iface = get_iface_by_handle(hid_dev_handle);

    HID_RETURN_ON_INVALID_ARG(iface);
    HID_RETURN_ON_INVALID_ARG(idle_rate);

    uint8_t tmp[1] = { 0xff };

    const hid_class_request_t get_idle = {
        .bRequest = HID_CLASS_SPECIFIC_REQ_GET_IDLE,
        .wValue = report_id,
        .wIndex = iface->dev_params.iface_num,
        .wLength = 1,
        .data = tmp
    };

    HID_RETURN_ON_ERROR( hid_class_request_get(iface->parent, &get_idle, NULL),
                         "HID class request transfer failure");

    *idle_rate = tmp[0];

    return ESP_OK;
}

esp_err_t hid_class_request_get_protocol(hid_host_device_handle_t hid_dev_handle,
        hid_report_protocol_t *protocol)
{
    hid_iface_t *iface = get_iface_by_handle(hid_dev_handle);

    HID_RETURN_ON_INVALID_ARG(iface);
    HID_RETURN_ON_INVALID_ARG(protocol);

    uint8_t tmp[1] = { 0xff };

    const hid_class_request_t get_proto = {
        .bRequest = HID_CLASS_SPECIFIC_REQ_GET_PROTOCOL,
        .wValue = 0,
        .wIndex = iface->dev_params.iface_num,
        .wLength = 1,
        .data = tmp
    };

    HID_RETURN_ON_ERROR( hid_class_request_get(iface->parent, &get_proto, NULL),
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
    hid_iface_t *iface = get_iface_by_handle(hid_dev_handle);

    HID_RETURN_ON_INVALID_ARG(iface);

    const hid_class_request_t set_report = {
        .bRequest = HID_CLASS_SPECIFIC_REQ_SET_REPORT,
        .wValue = (report_type << 8) | report_id,
        .wIndex = iface->dev_params.iface_num,
        .wLength = report_length,
        .data = report
    };

    return hid_class_request_set(iface->parent, &set_report);
}

esp_err_t hid_class_request_set_idle(hid_host_device_handle_t hid_dev_handle,
                                     uint8_t duration,
                                     uint8_t report_id)
{
    hid_iface_t *iface = get_iface_by_handle(hid_dev_handle);

    HID_RETURN_ON_INVALID_ARG(iface);

    const hid_class_request_t set_idle = {
        .bRequest = HID_CLASS_SPECIFIC_REQ_SET_IDLE,
        .wValue = (duration << 8) | report_id,
        .wIndex = iface->dev_params.iface_num,
        .wLength = 0,
        .data = NULL
    };

    return hid_class_request_set(iface->parent, &set_idle);
}

esp_err_t hid_class_request_set_protocol(hid_host_device_handle_t hid_dev_handle,
        hid_report_protocol_t protocol)
{
    hid_iface_t *iface = get_iface_by_handle(hid_dev_handle);

    HID_RETURN_ON_INVALID_ARG(iface);

    const hid_class_request_t set_proto = {
        .bRequest = HID_CLASS_SPECIFIC_REQ_SET_PROTOCOL,
        .wValue = protocol,
        .wIndex = iface->dev_params.iface_num,
        .wLength = 0,
        .data = NULL
    };

    return hid_class_request_set(iface->parent, &set_proto);
}
