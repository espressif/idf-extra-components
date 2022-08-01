/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LIBUSB_CALL static

#define LIBUSB_DT_DEVICE_SIZE           18
#define LIBUSB_DT_CONFIG_SIZE           9
#define LIBUSB_DT_INTERFACE_SIZE        9
#define LIBUSB_DT_ENDPOINT_SIZE         7
#define LIBUSB_DT_ENDPOINT_AUDIO_SIZE   9

enum libusb_error {
    LIBUSB_SUCCESS = 0,
    LIBUSB_ERROR_IO = -1,
    LIBUSB_ERROR_INVALID_PARAM = -2,
    LIBUSB_ERROR_ACCESS = -3,
    LIBUSB_ERROR_NO_DEVICE = -4,
    LIBUSB_ERROR_NOT_FOUND = -5,
    LIBUSB_ERROR_BUSY = -6,
    LIBUSB_ERROR_TIMEOUT = -7,
    LIBUSB_ERROR_OVERFLOW = -8,
    LIBUSB_ERROR_PIPE = -9,
    LIBUSB_ERROR_INTERRUPTED = -10,
    LIBUSB_ERROR_NO_MEM = -11,
    LIBUSB_ERROR_NOT_SUPPORTED = -12,
    LIBUSB_ERROR_OTHER = -99
};

enum libusb_descriptor_type {
    LIBUSB_DT_DEVICE = 0x01,
    LIBUSB_DT_CONFIG = 0x02,
    LIBUSB_DT_STRING = 0x03,
    LIBUSB_DT_INTERFACE = 0x04,
    LIBUSB_DT_ENDPOINT = 0x05,
    LIBUSB_DT_BOS = 0x0f,
    LIBUSB_DT_DEVICE_CAPABILITY = 0x10,
    LIBUSB_DT_HID = 0x21,
    LIBUSB_DT_REPORT = 0x22,
    LIBUSB_DT_PHYSICAL = 0x23,
    LIBUSB_DT_HUB = 0x29,
    LIBUSB_DT_SUPERSPEED_HUB = 0x2a,
    LIBUSB_DT_SS_ENDPOINT_COMPANION = 0x30
};

struct libusb_device_descriptor {
    uint8_t bLength;                    /**< Size of the descriptor in bytes */
    uint8_t bDescriptorType;            /**< DEVICE Descriptor Type */
    uint16_t bcdUSB;                    /**< USB Specification Release Number in Binary-Coded Decimal (i.e., 2.10 is 210H) */
    uint8_t bDeviceClass;               /**< Class code (assigned by the USB-IF) */
    uint8_t bDeviceSubClass;            /**< Subclass code (assigned by the USB-IF) */
    uint8_t bDeviceProtocol;            /**< Protocol code (assigned by the USB-IF) */
    uint8_t bMaxPacketSize0;            /**< Maximum packet size for endpoint zero (only 8, 16, 32, or 64 are valid) */
    uint16_t idVendor;                  /**< Vendor ID (assigned by the USB-IF) */
    uint16_t idProduct;                 /**< Product ID (assigned by the manufacturer) */
    uint16_t bcdDevice;                 /**< Device release number in binary-coded decimal */
    uint8_t iManufacturer;              /**< Index of string descriptor describing manufacturer */
    uint8_t iProduct;                   /**< Index of string descriptor describing product */
    uint8_t iSerialNumber;              /**< Index of string descriptor describing the device’s serial number */
    uint8_t bNumConfigurations;         /**< Number of possible configurations */
};

struct libusb_endpoint_descriptor {
    uint8_t bLength;                    /**< Size of the descriptor in bytes */
    uint8_t bDescriptorType;            /**< ENDPOINT Descriptor Type */
    uint8_t bEndpointAddress;           /**< The address of the endpoint on the USB device described by this descriptor */
    uint8_t bmAttributes;               /**< This field describes the endpoint’s attributes when it is configured using the bConfigurationValue. */
    uint16_t wMaxPacketSize;            /**< Maximum packet size this endpoint is capable of sending or receiving when this configuration is selected. */
    uint8_t bInterval;                  /**< Interval for polling Isochronous and Interrupt endpoints. Expressed in frames or microframes depending on the device operating speed (1 ms for Low-Speed and Full-Speed or 125 us for USB High-Speed and above). */
    uint8_t *extra;
    size_t extra_length;
};

struct libusb_interface_descriptor {
    uint8_t bLength;                    /**< Size of the descriptor in bytes */
    uint8_t bDescriptorType;            /**< INTERFACE Descriptor Type */
    uint8_t bInterfaceNumber;           /**< Number of this interface. */
    uint8_t bAlternateSetting;          /**< Value used to select this alternate setting for the interface identified in the prior field */
    uint8_t bNumEndpoints;              /**< Number of endpoints used by this interface (excluding endpoint zero). */
    uint8_t bInterfaceClass;            /**< Class code (assigned by the USB-IF) */
    uint8_t bInterfaceSubClass;         /**< Subclass code (assigned by the USB-IF) */
    uint8_t bInterfaceProtocol;         /**< Protocol code (assigned by the USB) */
    uint8_t iInterface;                 /**< Index of string descriptor describing this interface */
    uint8_t *extra;
    size_t extra_length;
    struct libusb_endpoint_descriptor *endpoint;
};

struct libusb_interface {
    size_t num_altsetting;
    struct libusb_interface_descriptor *altsetting;
};

struct libusb_config_descriptor {
    uint8_t bLength;                    /**< Size of the descriptor in bytes */
    uint8_t bDescriptorType;            /**< CONFIGURATION Descriptor Type */
    uint16_t wTotalLength;              /**< Total length of data returned for this configuration */
    uint8_t bNumInterfaces;             /**< Number of interfaces supported by this configuration */
    uint8_t bConfigurationValue;        /**< Value to use as an argument to the SetConfiguration() request to select this configuration */
    uint8_t iConfiguration;             /**< Index of string descriptor describing this configuration */
    uint8_t bmAttributes;               /**< Configuration characteristics */
    uint8_t bMaxPower;                  /**< Maximum power consumption of the USB device from the bus in this specific configuration when the device is fully operational. */
    uint8_t *extra;
    size_t extra_length;
    struct libusb_interface *interface;
};

typedef struct libusb_config_descriptor libusb_config_descriptor_t;
typedef struct libusb_interface_descriptor libusb_interface_descriptor_t;
typedef struct libusb_endpoint_descriptor libusb_endpoint_descriptor_t;
typedef struct libusb_interface libusb_interface_t;

struct libusb_ss_endpoint_companion_descriptor {
    uint32_t wBytesPerInterval;
};

struct libusb_device;
typedef struct libusb_device libusb_device;

struct libusb_device_handle;
typedef struct libusb_device_handle libusb_device_handle;

struct libusb_context;

typedef enum libusb_transfer_status {
    LIBUSB_TRANSFER_COMPLETED,
    LIBUSB_TRANSFER_CANCELLED,
    LIBUSB_TRANSFER_ERROR,
    LIBUSB_TRANSFER_NO_DEVICE,
    LIBUSB_TRANSFER_TIMED_OUT,
    LIBUSB_TRANSFER_STALL,
    LIBUSB_TRANSFER_OVERFLOW,
} libusb_status_t;

typedef struct libusb_iso_packet_descriptor {
    size_t length;
    size_t actual_length;
    libusb_status_t status;
} libusb_iso_packet_t;

struct libusb_transfer {
    libusb_device_handle *dev_handle;
    libusb_status_t status;
    uint8_t endpoint;
    uint8_t *buffer;
    size_t length;
    size_t actual_length;
    void *user_data;
    void (*callback)(struct libusb_transfer *);
    size_t timeout;
    size_t num_iso_packets;
    libusb_iso_packet_t iso_packet_desc[0];
};

typedef void (*libusb_transfer_cb)(struct libusb_transfer *transfer);

int libusb_init(struct libusb_context **ctx);

void libusb_exit(struct libusb_context *ctx);

int libusb_open(libusb_device *dev, libusb_device_handle **dev_handle);

void libusb_close(libusb_device_handle *dev_handle);

int32_t libusb_get_device_list(struct libusb_context *ctx, libusb_device ***list);

void libusb_free_device_list(libusb_device **list, int unref_devices);

int libusb_handle_events_completed(struct libusb_context *ctx, int *completed);

int libusb_control_transfer(libusb_device_handle *dev_handle,
                            uint8_t bmRequestType,
                            uint8_t bRequest,
                            uint16_t wValue,
                            uint16_t wIndex,
                            unsigned char *data,
                            uint16_t wLength,
                            unsigned int timeout);

void libusb_free_transfer(struct libusb_transfer *transfer);

int libusb_submit_transfer(struct libusb_transfer *transfer);

int libusb_cancel_transfer(struct libusb_transfer *transfer);

inline uint8_t *libusb_get_iso_packet_buffer_simple(struct libusb_transfer *transfer, uint32_t packet_id)
{
    if (packet_id >= transfer->num_iso_packets) {
        return NULL;
    }

    return &transfer->buffer[transfer->iso_packet_desc[0].length * packet_id];
}

struct libusb_transfer *libusb_alloc_transfer(int iso_packets);

inline void libusb_fill_iso_transfer(struct libusb_transfer *transfer,
                                     libusb_device_handle *dev,
                                     uint8_t bEndpointAddress,
                                     uint8_t *buffer,
                                     size_t total_transfer_size,
                                     size_t packets_per_transfer,
                                     libusb_transfer_cb callback,
                                     void *user_data,
                                     size_t timeout)
{
    transfer->dev_handle = dev;
    transfer->endpoint = bEndpointAddress;
    transfer->timeout = timeout;
    transfer->buffer = buffer;
    transfer->length = total_transfer_size;
    transfer->num_iso_packets = packets_per_transfer;
    transfer->user_data = user_data;
    transfer->callback = callback;
}

inline void libusb_fill_bulk_transfer (struct libusb_transfer *transfer,
                                       libusb_device_handle *dev,
                                       uint8_t bEndpointAddress,
                                       uint8_t *buffer,
                                       size_t length,
                                       libusb_transfer_cb callback,
                                       void *user_data,
                                       size_t timeout)
{
    transfer->dev_handle = dev;
    transfer->endpoint = bEndpointAddress;
    transfer->buffer = buffer;
    transfer->length = length;
    transfer->callback = callback;
    transfer->user_data = user_data;
    transfer->timeout = timeout;
    transfer->num_iso_packets = 0;
}

inline void libusb_fill_interrupt_transfer (struct libusb_transfer *transfer,
        libusb_device_handle *dev,
        uint8_t bEndpointAddress,
        uint8_t *buffer,
        size_t length,
        libusb_transfer_cb callback,
        void *user_data,
        size_t timeout)
{
    transfer->dev_handle = dev;
    transfer->endpoint = bEndpointAddress;
    transfer->buffer = buffer;
    transfer->length = length;
    transfer->callback = callback;
    transfer->user_data = user_data;
    transfer->timeout = timeout;
    transfer->num_iso_packets = 0;
}

inline void libusb_set_iso_packet_lengths(struct libusb_transfer *transfer, size_t length)
{
    for (uint32_t i = 0; i < transfer->num_iso_packets; i++) {
        transfer->iso_packet_desc[i].length = length;
    }
}

int libusb_set_interface_alt_setting(libusb_device_handle *dev_handle, int32_t inferface, int32_t alt_settings);

int libusb_get_ss_endpoint_companion_descriptor(struct libusb_context *ctx,
        const struct libusb_endpoint_descriptor *endpoint,
        struct libusb_ss_endpoint_companion_descriptor **ep_comp);

int libusb_get_device_descriptor(libusb_device *dev, struct libusb_device_descriptor *desc);

int libusb_get_config_descriptor(libusb_device *dev, uint8_t config_index, struct libusb_config_descriptor **config);

void libusb_free_config_descriptor(struct libusb_config_descriptor *config);

int libusb_get_string_descriptor_ascii(libusb_device_handle *dev_handle, uint8_t desc_index, unsigned char *data, int length);

void libusb_free_ss_endpoint_companion_descriptor(struct libusb_ss_endpoint_companion_descriptor *desc);

int8_t libusb_get_bus_number(libusb_device *device);

int8_t libusb_get_device_address(libusb_device *device);

libusb_device *libusb_ref_device(libusb_device *dev);

void libusb_unref_device(libusb_device *dev);

int libusb_claim_interface(libusb_device_handle *dev_handle, int interface);

int libusb_release_interface(libusb_device_handle *dev_handle, int interface);

int libusb_attach_kernel_driver(libusb_device_handle *dev_handle, int interface_number);

int libusb_detach_kernel_driver(libusb_device_handle *dev_handle, int interface_number);

#ifdef __cplusplus
}
#endif
