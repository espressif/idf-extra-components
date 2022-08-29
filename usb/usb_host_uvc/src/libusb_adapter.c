/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "libusb.h"
#include <stdio.h>
#include <wchar.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/queue.h>
#include "esp_pthread.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"
#include "usb/usb_types_stack.h"
#include "usb/usb_helpers.h"
#include "descriptor.h"
#include "sdkconfig.h"
#include "libuvc/libuvc.h"
#include "libuvc/libuvc_internal.h"
#include "libuvc_adapter.h"

#define TAG "libusb adapter"

#define GOTO_ON_FALSE(exp) ESP_GOTO_ON_FALSE(exp, ESP_ERR_NO_MEM, fail, TAG, "")

#define RETURN_ON_ERROR(exp) ESP_RETURN_ON_ERROR(exp, TAG, "err: %s", esp_err_to_name(err_rc_))

#define GOTO_ON_ERROR(exp) ESP_GOTO_ON_ERROR(exp, fail, TAG, "")

#define RETURN_ON_ERROR_LIBUSB(exp) do {    \
    esp_err_t _err_ = (exp);                \
    if(_err_ != ESP_OK) {                   \
        return esp_to_libusb_error(_err_);  \
    }                                       \
} while(0)

#define UVC_ENTER_CRITICAL()    portENTER_CRITICAL(&s_uvc_lock)
#define UVC_EXIT_CRITICAL()     portEXIT_CRITICAL(&s_uvc_lock)

#define COUNT_OF(array) (sizeof(array) / sizeof(array[0]))

typedef struct {
    usb_transfer_t *xfer;
    struct libusb_transfer libusb_xfer;
} uvc_transfer_t;

typedef struct opened_camera {
    uint8_t address;
    uint8_t open_count;
    uint16_t endpoint_mps; // interrupt endpoint
    uint8_t active_alt_setting;
    usb_device_handle_t handle;
    usb_transfer_t *control_xfer;
    SemaphoreHandle_t transfer_done;
    usb_transfer_status_t transfer_status;
    STAILQ_ENTRY(opened_camera) tailq_entry;
} uvc_camera_t;

typedef struct {
    usb_host_client_handle_t client;
    volatile bool delete_client_task;
    SemaphoreHandle_t client_task_deleted;
    STAILQ_HEAD(opened_devs, opened_camera) opened_devices_tailq;
} uvc_driver_t;

static portMUX_TYPE s_uvc_lock = portMUX_INITIALIZER_UNLOCKED;
static uvc_driver_t *s_uvc_driver;

static libuvc_adapter_config_t s_config = {
    .create_background_task = true,
    .task_priority = 5,
    .stack_size = 4096,
    .callback = NULL,
};

static const usb_standard_desc_t *next_interface_desc(const usb_standard_desc_t *desc, size_t len, int *offset)
{
    return usb_parse_next_descriptor_of_type(desc, len, USB_W_VALUE_DT_INTERFACE, offset);
}

static const usb_standard_desc_t *next_endpoint_desc(const usb_standard_desc_t *desc, size_t len, int *offset)
{
    return usb_parse_next_descriptor_of_type(desc, len, USB_B_DESCRIPTOR_TYPE_ENDPOINT, (int *)offset);
}

// Find endpoint number under specified interface.
static esp_err_t find_endpoint_of_interface(const usb_config_desc_t *config_desc, uint8_t interface, uint8_t *endpoint)
{
    int offset = 0;
    size_t total_length = config_desc->wTotalLength;
    const usb_standard_desc_t *next_desc = (const usb_standard_desc_t *)config_desc;

    next_desc = next_interface_desc(next_desc, total_length, &offset);

    while ( next_desc ) {

        const usb_intf_desc_t *ifc_desc = (const usb_intf_desc_t *)next_desc;

        if ( ifc_desc->bInterfaceNumber == interface && ifc_desc->bNumEndpoints != 0) {
            next_desc = next_endpoint_desc(next_desc, total_length, &offset);
            if (next_desc == NULL) {
                return ESP_ERR_NOT_SUPPORTED;
            }
            *endpoint = ((const usb_ep_desc_t *)next_desc)->bEndpointAddress;
            return ESP_OK;
        }

        next_desc = next_interface_desc(next_desc, total_length, &offset);
    };

    return ESP_ERR_NOT_SUPPORTED;
}


static uint16_t get_interupt_endpoint_mps(const usb_config_desc_t *config_desc)
{
    int offset = 0;
    size_t total_length = config_desc->wTotalLength;
    const usb_standard_desc_t *next_desc = (const usb_standard_desc_t *)config_desc;

    while ( (next_desc = next_endpoint_desc(next_desc, total_length, &offset)) ) {
        const usb_ep_desc_t *ep_desc = (const usb_ep_desc_t *)next_desc;
        if (USB_EP_DESC_GET_XFERTYPE(ep_desc) == USB_BM_ATTRIBUTES_XFER_INT) {
            return ep_desc->wMaxPacketSize;
        }
    };

    return 32;
}

void libuvc_adapter_set_config(libuvc_adapter_config_t *config)
{
    if (config == NULL) {
        return;
    }

    s_config = *config;
}

static void print_str_desc(const usb_str_desc_t *desc, const char *name)
{
    wchar_t str[32];
    size_t str_len = MIN((desc->bLength - USB_STANDARD_DESC_SIZE) / 2, COUNT_OF(str) - 1);

    // Copy utf-16 to wchar_t array
    for (size_t i = 0; i < str_len; i++) {
        str[i] = desc->wData[i];
    }
    str[str_len] = '\0';

    wprintf(L"%s: %S \n", name, str);
}

static void print_string_descriptors(usb_device_info_t *dev_info)
{
    printf("*** String Descriptors ***\n");

    if (dev_info->str_desc_product) {
        print_str_desc(dev_info->str_desc_product, "iProduct");
    }
    if (dev_info->str_desc_manufacturer) {
        print_str_desc(dev_info->str_desc_manufacturer, "iManufacturer");
    }
    if (dev_info->str_desc_serial_num) {
        print_str_desc(dev_info->str_desc_serial_num, "iSerialNumber");
    }
}

esp_err_t libuvc_adapter_print_descriptors(uvc_device_handle_t *device)
{
    uvc_camera_t *camera = (uvc_camera_t *)(device->usb_devh);
    const usb_config_desc_t *config_desc;
    const usb_device_desc_t *device_desc;
    usb_device_info_t dev_info;

    RETURN_ON_ERROR( usb_host_get_device_descriptor(camera->handle, &device_desc) );
    RETURN_ON_ERROR( usb_host_get_active_config_descriptor(camera->handle, &config_desc) );
    RETURN_ON_ERROR( usb_host_device_info(camera->handle, &dev_info) );

    usb_print_device_descriptor(device_desc);
    usb_print_config_descriptor(config_desc, print_usb_class_descriptors);
    print_string_descriptors(&dev_info);

    return ESP_OK;
}

esp_err_t libuvc_adapter_handle_events(uint32_t timeout_ms)
{
    if (s_uvc_driver == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return usb_host_client_handle_events(s_uvc_driver->client, pdMS_TO_TICKS(timeout_ms));
}

static int esp_to_libusb_error(esp_err_t err)
{
    switch (err) {
    case ESP_ERR_TIMEOUT:   return LIBUSB_ERROR_TIMEOUT;
    case ESP_ERR_NO_MEM:    return LIBUSB_ERROR_NO_MEM;
    case ESP_FAIL:          return LIBUSB_ERROR_PIPE;
    case ESP_OK:            return LIBUSB_SUCCESS;
    default:                return LIBUSB_ERROR_OTHER;
    }
}

static enum libusb_transfer_status eps_to_libusb_status(usb_transfer_status_t esp_status)
{
    switch (esp_status) {
    case USB_TRANSFER_STATUS_COMPLETED: return LIBUSB_TRANSFER_COMPLETED;
    case USB_TRANSFER_STATUS_TIMED_OUT: return LIBUSB_TRANSFER_TIMED_OUT;
    case USB_TRANSFER_STATUS_CANCELED:  return LIBUSB_TRANSFER_CANCELLED;
    case USB_TRANSFER_STATUS_NO_DEVICE: return LIBUSB_TRANSFER_NO_DEVICE;
    case USB_TRANSFER_STATUS_OVERFLOW:  return LIBUSB_TRANSFER_OVERFLOW;
    case USB_TRANSFER_STATUS_STALL:     return LIBUSB_TRANSFER_STALL;
    default: return LIBUSB_TRANSFER_ERROR;
    }
}

static void usb_client_event_handler(void *arg)
{
    ulTaskNotifyTake(false, pdMS_TO_TICKS(1000));

    do {
        usb_host_client_handle_events(s_uvc_driver->client, pdMS_TO_TICKS(50));
    } while (!s_uvc_driver->delete_client_task);

    xSemaphoreGive(s_uvc_driver->client_task_deleted);
    vTaskDelete(NULL);
}


static void client_event_cb(const usb_host_client_event_msg_t *event, void *arg)
{
    if (s_config.callback) {
        switch (event->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            ESP_LOGD(TAG, "USB device connected");
            s_config.callback(UVC_DEVICE_CONNECTED);
            break;

        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            ESP_LOGD(TAG, "USB device disconnected");
            s_config.callback(UVC_DEVICE_DISCONNECTED);
            break;

        default:
            break;
        }
    }
}

int libusb_init(struct libusb_context **ctx)
{
    uvc_driver_t *driver = NULL;
    TaskHandle_t client_task_handle = NULL;
    esp_err_t ret = ESP_ERR_NO_MEM;

    usb_host_client_config_t client_config = {
        .async.client_event_callback = client_event_cb,
        .async.callback_arg = NULL,
        .max_num_event_msg = 5,
    };

    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    esp_pthread_set_cfg(&cfg);

    GOTO_ON_FALSE( driver = calloc(1, sizeof(uvc_driver_t)) );
    GOTO_ON_ERROR( usb_host_client_register(&client_config, &driver->client) );
    GOTO_ON_FALSE( driver->client_task_deleted = xSemaphoreCreateBinary() );

    STAILQ_INIT(&driver->opened_devices_tailq);

    if (s_config.create_background_task) {
        GOTO_ON_FALSE( xTaskCreate(usb_client_event_handler, "uvc_events", s_config.stack_size,
                                   NULL, s_config.task_priority, &client_task_handle) );
    }

    UVC_ENTER_CRITICAL();
    if (s_uvc_driver != NULL) {
        UVC_EXIT_CRITICAL();
        ret = ESP_ERR_TIMEOUT;
        goto fail;
    }
    s_uvc_driver = driver;
    UVC_EXIT_CRITICAL();

    if (client_task_handle) {
        xTaskNotifyGive(client_task_handle);
    }

    *ctx = (struct libusb_context *)driver;
    return LIBUSB_SUCCESS;

fail:
    if (driver) {
        if (driver->client) {
            usb_host_client_deregister(driver->client);
        };
        if (driver->client_task_deleted) {
            vSemaphoreDelete(driver->client_task_deleted);
        }
        free(driver);
    }
    if (client_task_handle) {
        vTaskDelete(client_task_handle);
    }
    return esp_to_libusb_error(ret);
}

void libusb_exit(struct libusb_context *ctx)
{
    uvc_driver_t *driver = (uvc_driver_t *)ctx;
    UVC_ENTER_CRITICAL();
    if (driver == NULL) {
        UVC_EXIT_CRITICAL();
        return;

    }
    UVC_EXIT_CRITICAL();

    if (s_config.create_background_task) {
        driver->delete_client_task = true;
    }

    usb_host_client_unblock(driver->client);
    xSemaphoreTake(s_uvc_driver->client_task_deleted, portMAX_DELAY);
    if (usb_host_client_deregister(driver->client) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deregister USB client");
    }

    vSemaphoreDelete(s_uvc_driver->client_task_deleted);
    s_uvc_driver = NULL;
    free(driver);
}

int32_t libusb_get_device_list(struct libusb_context *ctx, libusb_device ***list)
{
    static const size_t DEV_LIST_SIZE = 5;

    int actual_count;
    uint8_t dev_addr_list[DEV_LIST_SIZE];
    usb_host_device_addr_list_fill(DEV_LIST_SIZE, dev_addr_list, &actual_count);

    libusb_device **dev_list = calloc(actual_count + 1, sizeof(libusb_device *));
    if (dev_list == NULL) {
        return 0;
    }

    for (size_t i = 0; i < actual_count; i++) {
        dev_list[i] = (libusb_device *)(uint32_t)dev_addr_list[i];
    }
    *list = (libusb_device **)dev_list;
    return actual_count;
}

void libusb_free_device_list(libusb_device **list, int unref_devices)
{
    free(list);
}

// As opposed to LIBUSB, USB_HOST library does not allows to open devices recursively and get descriptors without opening device.
// Thus, libusb_adapter keeps track of how many times the device is opened and closes it only when the count reaches zero.
static esp_err_t open_device_if_closed(uint8_t device_addr, uvc_camera_t **handle)
{
    uvc_camera_t *device;
    esp_err_t ret = ESP_ERR_NO_MEM;

    uvc_camera_t *new_device = calloc(1, sizeof(uvc_camera_t));
    if (new_device == NULL) {
        return ESP_ERR_NO_MEM;
    }

    UVC_ENTER_CRITICAL();

    STAILQ_FOREACH(device, &s_uvc_driver->opened_devices_tailq, tailq_entry) {
        if (device_addr == device->address) {
            *handle = device;
            device->open_count++;
            UVC_EXIT_CRITICAL();
            free(new_device);
            return ESP_OK;
        }
    }

    new_device->open_count++;
    new_device->address = device_addr;
    STAILQ_INSERT_TAIL(&s_uvc_driver->opened_devices_tailq, new_device, tailq_entry);

    UVC_EXIT_CRITICAL();

    GOTO_ON_ERROR( usb_host_device_open(s_uvc_driver->client, device_addr, &new_device->handle) );
    GOTO_ON_ERROR( usb_host_transfer_alloc(128, 0, &new_device->control_xfer) );
    GOTO_ON_FALSE( new_device->transfer_done = xSemaphoreCreateBinary() );

    *handle = new_device;
    return ESP_OK;

fail:
    UVC_ENTER_CRITICAL();
    STAILQ_REMOVE(&s_uvc_driver->opened_devices_tailq, new_device, opened_camera, tailq_entry);
    UVC_EXIT_CRITICAL();
    free(new_device);
    return ret;
}

static esp_err_t close_device(uvc_camera_t *device)
{
    bool close = false;

    UVC_ENTER_CRITICAL();
    if (--device->open_count == 0) {
        STAILQ_REMOVE(&s_uvc_driver->opened_devices_tailq, device, opened_camera, tailq_entry);
        close = true;
    }
    UVC_EXIT_CRITICAL();

    if (close) {
        RETURN_ON_ERROR( usb_host_device_close(s_uvc_driver->client, device->handle) );
        RETURN_ON_ERROR( usb_host_transfer_free(device->control_xfer) );
        vSemaphoreDelete(device->transfer_done);
        free(device);
    }

    return LIBUSB_SUCCESS;
}

int libusb_open(libusb_device *dev, libusb_device_handle **dev_handle)
{
    uint8_t device_addr = (uint8_t)(uint32_t)dev;
    uvc_camera_t *device;

    RETURN_ON_ERROR_LIBUSB( open_device_if_closed(device_addr, &device) );

    *dev_handle = (libusb_device_handle *)device;
    return LIBUSB_SUCCESS;
}

void libusb_close(libusb_device_handle *dev_handle)
{
    esp_err_t err = close_device((uvc_camera_t *)dev_handle);
    if (err) {
        ESP_LOGE(TAG, "Failed to close device");
    }
}

void libusb_free_transfer(struct libusb_transfer *transfer)
{
    uvc_transfer_t *trans = __containerof(transfer, uvc_transfer_t, libusb_xfer);
    usb_host_transfer_free(trans->xfer);
    free(trans);
}

struct libusb_transfer *libusb_alloc_transfer(int iso_packets)
{
    size_t alloc_size = sizeof(uvc_transfer_t) +
                        sizeof(struct libusb_iso_packet_descriptor) * iso_packets;

    uvc_transfer_t *xfer = calloc(1, alloc_size);
    if (xfer == NULL) {
        return NULL;
    }

    return &xfer->libusb_xfer;
}

static inline bool is_in_endpoint(uint8_t endpoint)
{
    return endpoint & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK ? true : false;
}

// Copies data from usb_transfer_t back to libusb_transfer and invokes user provided callback
void transfer_cb(usb_transfer_t *xfer)
{
    uvc_transfer_t *trans = xfer->context;
    struct libusb_transfer *libusb_trans = &trans->libusb_xfer;

    size_t isoc_actual_length = 0;

    for (int i = 0; i < xfer->num_isoc_packets; i++) {
        libusb_trans->iso_packet_desc[i].actual_length = xfer->isoc_packet_desc[i].actual_num_bytes;
        libusb_trans->iso_packet_desc[i].status = eps_to_libusb_status(xfer->isoc_packet_desc[i].status);

        if (libusb_trans->iso_packet_desc[i].status == LIBUSB_TRANSFER_COMPLETED) {
            isoc_actual_length += xfer->isoc_packet_desc[i].actual_num_bytes;
        }
    }

    libusb_trans->status = eps_to_libusb_status(xfer->status);
    libusb_trans->actual_length = xfer->num_isoc_packets ? isoc_actual_length : xfer->actual_num_bytes;

    if (is_in_endpoint(libusb_trans->endpoint)) {
        memcpy(libusb_trans->buffer, xfer->data_buffer, libusb_trans->length);
    }

    libusb_trans->callback(libusb_trans);
}

// This function copies libusb_transfer data into usb_transfer_t structure
int libusb_submit_transfer(struct libusb_transfer *libusb_trans)
{
    uvc_transfer_t *trans = __containerof(libusb_trans, uvc_transfer_t, libusb_xfer);
    esp_err_t err;

    int length = libusb_trans->length;
    int num_iso_packets = libusb_trans->num_iso_packets;
    uvc_camera_t *device = (uvc_camera_t *)libusb_trans->dev_handle;

    // Workaround: libuvc submits interrupt INTR transfers with transfer size
    // of 32 bytes, event though MSP of the endpoint might be 64.
    // Make in transfer rounded up to MSP of interrupt endpoint.
    // ISO transfers should be effected by this, as there are supposed to be 512 bytes long
    if (is_in_endpoint(libusb_trans->endpoint)) {
        length = usb_round_up_to_mps(length, device->endpoint_mps);
    }

    // Transfers are allocated/reallocated based on transfer size, as libusb
    // doesn't store buffers in DMA capable region
    if (!trans->xfer || trans->xfer->data_buffer_size < libusb_trans->length) {
        if (trans->xfer) {
            usb_host_transfer_free(trans->xfer);
        }
        err = usb_host_transfer_alloc(length, num_iso_packets, &trans->xfer);
        if (err) {
            ESP_LOGE(TAG, "Failed to allocate transfer with length: %u", length);
            return esp_to_libusb_error(err);
        }
    }

    if (!is_in_endpoint(libusb_trans->endpoint)) {
        memcpy(trans->xfer->data_buffer, libusb_trans->buffer, libusb_trans->length);
    }

    trans->xfer->device_handle = device->handle;
    trans->xfer->bEndpointAddress = libusb_trans->endpoint;
    trans->xfer->timeout_ms = libusb_trans->timeout;
    trans->xfer->callback = transfer_cb;
    trans->xfer->num_bytes = length;
    trans->xfer->context = trans;

    for (int i = 0; i < num_iso_packets; i++) {
        trans->xfer->isoc_packet_desc[i].num_bytes = libusb_trans->iso_packet_desc[i].length;
    }

    err = usb_host_transfer_submit(trans->xfer);
    return esp_to_libusb_error(err);
}

int libusb_cancel_transfer(struct libusb_transfer *transfer)
{
    return 0;
}

static bool is_in_request(uint8_t bmRequestType)
{
    return (bmRequestType & USB_BM_REQUEST_TYPE_DIR_IN) != 0 ? true : false;

}

static bool is_out_request(uint8_t bmRequestType)
{
    return (bmRequestType & USB_BM_REQUEST_TYPE_DIR_IN) == 0 ? true : false;
}

static void common_xfer_cb(usb_transfer_t *transfer)
{
    uvc_camera_t *device = (uvc_camera_t *)transfer->context;

    if (transfer->status != USB_TRANSFER_STATUS_COMPLETED) {
        ESP_EARLY_LOGE("Transfer failed", "Status %d", transfer->status);
    }

    device->transfer_status = transfer->status;
    xSemaphoreGive(device->transfer_done);
}

static esp_err_t wait_for_transmition_done(usb_transfer_t *xfer)
{
    uvc_camera_t *device = (uvc_camera_t *)xfer->context;
    BaseType_t received = xSemaphoreTake(device->transfer_done, pdMS_TO_TICKS(xfer->timeout_ms));

    if (received != pdTRUE) {
        usb_host_endpoint_halt(xfer->device_handle, xfer->bEndpointAddress);
        usb_host_endpoint_flush(xfer->device_handle, xfer->bEndpointAddress);
        xSemaphoreTake(device->transfer_done, portMAX_DELAY);
        return ESP_ERR_TIMEOUT;
    }

    if (device->transfer_status != USB_TRANSFER_STATUS_COMPLETED) {
        printf("transfer_status: %d", device->transfer_status);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static int control_transfer(libusb_device_handle *dev_handle,
                            usb_setup_packet_t *request,
                            unsigned char *data,
                            unsigned int timeout)
{
    return libusb_control_transfer(dev_handle, request->bmRequestType, request->bRequest,
                                   request->wValue, request->wIndex, data,
                                   request->wLength, timeout);
}

int libusb_control_transfer(libusb_device_handle *dev_handle,
                            uint8_t bmRequestType,
                            uint8_t bRequest,
                            uint16_t wValue,
                            uint16_t wIndex,
                            unsigned char *data,
                            uint16_t wLength,
                            unsigned int timeout)
{
    uvc_camera_t *device = (uvc_camera_t *)dev_handle;
    usb_transfer_t *xfer = device->control_xfer;
    usb_setup_packet_t *ctrl_req = (usb_setup_packet_t *)xfer->data_buffer;

    ctrl_req->bmRequestType = bmRequestType;
    ctrl_req->bRequest = bRequest;
    ctrl_req->wValue = wValue;
    ctrl_req->wIndex = wIndex;
    ctrl_req->wLength = wLength;

    xfer->device_handle = device->handle;
    xfer->bEndpointAddress = 0;
    xfer->callback = common_xfer_cb;
    xfer->timeout_ms = MAX(timeout, 100);
    xfer->num_bytes = USB_SETUP_PACKET_SIZE + wLength;
    xfer->context = device;

    if (is_out_request(bmRequestType)) {
        memcpy(xfer->data_buffer + sizeof(usb_setup_packet_t), data, wLength);
    }

    RETURN_ON_ERROR_LIBUSB( usb_host_transfer_submit_control(s_uvc_driver->client, xfer) );
    RETURN_ON_ERROR_LIBUSB( wait_for_transmition_done(xfer) );

    if (is_in_request(bmRequestType)) {
        memcpy(data, xfer->data_buffer + sizeof(usb_setup_packet_t), wLength);
    }

    return xfer->actual_num_bytes;
}

int libusb_get_device_descriptor(libusb_device *dev, struct libusb_device_descriptor *desc)
{
    uint8_t device_addr = (uint8_t)(uint32_t)dev;
    const usb_device_desc_t *device_desc;
    uvc_camera_t *device;

    // Open device if closed, as USB host doesn't allow to get descriptor without opening device
    RETURN_ON_ERROR_LIBUSB( open_device_if_closed(device_addr, &device) );

    RETURN_ON_ERROR_LIBUSB( usb_host_get_device_descriptor(device->handle, &device_desc) );

    desc->bLength =         device_desc->bLength;
    desc->bDescriptorType = device_desc->bDescriptorType;
    desc->bcdUSB =          device_desc->bcdUSB;
    desc->bDeviceClass =    device_desc->bDeviceClass;
    desc->bDeviceSubClass = device_desc->bDeviceSubClass;
    desc->bDeviceProtocol = device_desc->bDeviceProtocol;
    desc->bMaxPacketSize0 = device_desc->bMaxPacketSize0;
    desc->idVendor =        device_desc->idVendor;
    desc->idProduct =       device_desc->idProduct;
    desc->bcdDevice =       device_desc->bcdDevice;
    desc->iManufacturer =   device_desc->iManufacturer;
    desc->iProduct =        device_desc->iProduct;
    desc->iSerialNumber =   device_desc->iSerialNumber;
    desc->bNumConfigurations = device_desc->bNumConfigurations;

    RETURN_ON_ERROR_LIBUSB( close_device(device) );

    return LIBUSB_SUCCESS;
}

int libusb_get_config_descriptor(libusb_device *dev,
                                 uint8_t config_index,
                                 struct libusb_config_descriptor **config)
{
    uint8_t device_addr = (uint8_t)(uint32_t)dev;
    const usb_config_desc_t *config_desc;
    uvc_camera_t *device;

    // Open device if closed, as USB host doesn't allow to get descriptor without opening device
    RETURN_ON_ERROR_LIBUSB( open_device_if_closed(device_addr, &device) );

    RETURN_ON_ERROR_LIBUSB( usb_host_get_active_config_descriptor(device->handle, &config_desc) );

    int res = raw_desc_to_libusb_config(&config_desc->val[0], config_desc->wTotalLength, config);

    device->endpoint_mps = get_interupt_endpoint_mps(config_desc);

    RETURN_ON_ERROR_LIBUSB( close_device(device) );

    return res;
}

void libusb_free_config_descriptor(struct libusb_config_descriptor *config)
{
    clear_config_descriptor(config);
    free(config);
}

int libusb_get_string_descriptor_ascii(libusb_device_handle *dev_handle,
                                       uint8_t desc_index,
                                       unsigned char *data,
                                       int length)
{
#define US_LANG_ID 0x409
    usb_setup_packet_t ctrl_req;
    USB_SETUP_PACKET_INIT_GET_STR_DESC(&ctrl_req, desc_index, US_LANG_ID, length);
    return control_transfer(dev_handle, &ctrl_req, data, 1000);
}

int libusb_get_ss_endpoint_companion_descriptor(struct libusb_context *ctx,
        const struct libusb_endpoint_descriptor *endpoint,
        struct libusb_ss_endpoint_companion_descriptor **ep_comp)
{
    return 0;
}

void libusb_free_ss_endpoint_companion_descriptor(struct libusb_ss_endpoint_companion_descriptor *ep_comp)
{

}

libusb_device *libusb_ref_device(libusb_device *dev)
{
    return dev;
}

void libusb_unref_device(libusb_device *dev)
{

}

int libusb_claim_interface(libusb_device_handle *dev_handle, int interface)
{
    uvc_camera_t *device = (uvc_camera_t *)dev_handle;

    // Alternate interface will be claimed in libusb_set_interface_alt_setting function,
    // as libusb only support claming interface without alternate settings.
    return esp_to_libusb_error( usb_host_interface_claim(s_uvc_driver->client, device->handle, interface, 0) );
}

int libusb_release_interface(libusb_device_handle *dev_handle, int interface)
{
    uvc_camera_t *device = (uvc_camera_t *)dev_handle;
    const usb_config_desc_t *config_desc;
    uint8_t endpoint;

    RETURN_ON_ERROR_LIBUSB( usb_host_get_active_config_descriptor(device->handle, &config_desc) );

    RETURN_ON_ERROR_LIBUSB( find_endpoint_of_interface(config_desc, interface, &endpoint) );

    // Cancel any ongoing transfers before releasing interface
    usb_host_endpoint_halt(device->handle,  endpoint);
    usb_host_endpoint_flush(device->handle, endpoint);
    usb_host_endpoint_clear(device->handle, endpoint);

    return esp_to_libusb_error( usb_host_interface_release(s_uvc_driver->client, device->handle, interface) );
}

int libusb_set_interface_alt_setting(libusb_device_handle *dev_handle, int32_t inferface, int32_t alt_settings)
{
    uvc_camera_t *device = (uvc_camera_t *)dev_handle;
    usb_host_client_handle_t client = s_uvc_driver->client;
    uint8_t data[sizeof(usb_setup_packet_t)];
    usb_setup_packet_t request;

    // Setting alternate interface 0.0 is special case in UVC specs.
    // No interface is to be released, just send control transfer.
    if (inferface != 0 || alt_settings != 0) {
        RETURN_ON_ERROR_LIBUSB( usb_host_interface_release(client, device->handle, inferface) );
        RETURN_ON_ERROR_LIBUSB( usb_host_interface_claim(client, device->handle, inferface, alt_settings) );
    }

    USB_SETUP_PACKET_INIT_SET_INTERFACE(&request, inferface, alt_settings);
    int result = control_transfer(dev_handle, &request, data, 2000);
    return result > 0 ? LIBUSB_SUCCESS : result;
}

int libusb_attach_kernel_driver(libusb_device_handle *dev_handle, int interface_number)
{
    return 0;
}

int libusb_detach_kernel_driver(libusb_device_handle *dev_handle, int interface_number)
{
    return 0;
}

int libusb_handle_events_completed(struct libusb_context *ctx, int *completed)
{
    // USB events are handled either in client task or by user invoking libuvc_adapter_handle_events,
    // as LIBUVC calls this handler only after opening device. USB Host requires to call client handler
    // prior to opening device in order to receive USB_HOST_CLIENT_EVENT_NEW_DEV event.
    vTaskDelay(pdMS_TO_TICKS(1000));
    return 0;
}

int8_t libusb_get_bus_number(libusb_device *device)
{
    return 0;
}

int8_t libusb_get_device_address(libusb_device *device)
{
    // Device addres is stored directly in libusb_device
    return (uint8_t)(uint32_t)device;
}
