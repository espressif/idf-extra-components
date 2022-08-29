/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <endian.h>

#include "libusb.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"
#include "sys/param.h"

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
} desc_header_t;

typedef struct {
    const uint8_t *begin;
    uint8_t **data;
    size_t *len;
} extra_data_t;

#define DESC_HEADER_LENGTH  2
#define USB_MAXENDPOINTS    32
#define USB_MAXINTERFACES   32
#define USB_MAXCONFIG       8

#define TAG "DESC"

#define USB_DESC_ATTR           __attribute__((packed))


#define LIBUSB_GOTO_ON_ERROR(exp) do {      \
    int _res_ = exp;                        \
    if(_res_ != LIBUSB_SUCCESS) {           \
        ret = _res_;                        \
        goto cleanup;                       \
    }                                       \
} while(0)

#define LIBUSB_GOTO_ON_FALSE(exp) do {      \
    if((exp) == 0) {                        \
        goto cleanup;                       \
    }                                       \
} while(0)

static const usb_intf_desc_t *next_interface_desc(const usb_intf_desc_t *desc, size_t len, int *offset)
{
    return (const usb_intf_desc_t *) usb_parse_next_descriptor_of_type(
               (const usb_standard_desc_t *)desc, len, USB_B_DESCRIPTOR_TYPE_INTERFACE, offset);
}

void copy_config_desc(libusb_config_descriptor_t *libusb_desc, const usb_config_desc_t *idf_desc)
{
    libusb_desc->bLength = idf_desc->bLength;
    libusb_desc->bDescriptorType = idf_desc->bDescriptorType;
    libusb_desc->wTotalLength = idf_desc->wTotalLength;
    libusb_desc->bNumInterfaces = idf_desc->bNumInterfaces;
    libusb_desc->bConfigurationValue = idf_desc->bConfigurationValue;
    libusb_desc->iConfiguration = idf_desc->iConfiguration;
    libusb_desc->bmAttributes = idf_desc->bmAttributes;
    libusb_desc->bMaxPower = idf_desc->bMaxPower;
}

void copy_interface_desc(libusb_interface_descriptor_t *libusb_desc, const usb_intf_desc_t *idf_desc)
{
    libusb_desc->bLength = idf_desc->bLength;
    libusb_desc->bDescriptorType = idf_desc->bDescriptorType;
    libusb_desc->bInterfaceNumber = idf_desc->bInterfaceNumber;
    libusb_desc->bAlternateSetting = idf_desc->bAlternateSetting;
    libusb_desc->bNumEndpoints = idf_desc->bNumEndpoints;
    libusb_desc->bInterfaceClass = idf_desc->bInterfaceClass;
    libusb_desc->bInterfaceSubClass = idf_desc->bInterfaceSubClass;
    libusb_desc->bInterfaceProtocol = idf_desc->bInterfaceProtocol;
    libusb_desc->iInterface = idf_desc->iInterface;
}

void copy_endpoint_desc(libusb_endpoint_descriptor_t *libusb_desc, const usb_ep_desc_t *idf_desc)
{
    libusb_desc->bLength = idf_desc->bLength;
    libusb_desc->bDescriptorType = idf_desc->bDescriptorType;
    libusb_desc->bEndpointAddress = idf_desc->bEndpointAddress;
    libusb_desc->bmAttributes = idf_desc->bmAttributes;
    libusb_desc->wMaxPacketSize = idf_desc->wMaxPacketSize;
    libusb_desc->bInterval = idf_desc->bInterval;
}

static void set_extra_data(extra_data_t *extra, uint8_t **extra_data, size_t *extra_len, const uint8_t *begin)
{
    extra->data = extra_data;
    extra->len = extra_len;
    extra->begin = begin;
}

// Copies extra data to previously provided memory.
// The function allocates or realllocates memory depending on provided pointer
static libusb_status_t add_extra_data(extra_data_t *extra, const void *end)
{
    uint8_t *new_memory = NULL;

    int new_size = (int)((uint8_t *)end - extra->begin);

    if (new_size > 0) {
        if (*extra->data == NULL) {
            new_memory = malloc(new_size);
        } else {
            new_memory = realloc(*extra->data, *extra->len + new_size);
        }

        if (!new_memory) {
            return LIBUSB_ERROR_NO_MEM;
        }

        memcpy(new_memory + *extra->len, extra->begin, new_size);
        *extra->data = new_memory;
        *extra->len += new_size;
    }

    return LIBUSB_SUCCESS;
}

void clear_config_descriptor(libusb_config_descriptor_t *config)
{
    if (config) {
        if (config->interface) {
            for (int i = 0; i < config->bNumInterfaces; i++) {
                libusb_interface_t *interface = &config->interface[i];
                if (interface->altsetting) {
                    for (int j = 0; j < interface->num_altsetting; j++) {
                        libusb_interface_descriptor_t *alt = &interface->altsetting[j];
                        if (alt->endpoint) {
                            for (int ep = 0; ep < alt->bNumEndpoints; ep++) {
                                free(alt->endpoint[ep].extra);
                            }
                            free(alt->endpoint);
                        }
                        free(alt->extra);
                    }
                    free(interface->altsetting);
                }
            }
            free(config->interface);
        }
        free(config->extra);
    }
}

int parse_configuration(libusb_config_descriptor_t *config, const uint8_t *buffer, int size)
{
    int offset = 0;
    extra_data_t extra = { 0 };
    const usb_ep_desc_t *ep_desc;
    const usb_config_desc_t *config_start = (const usb_config_desc_t *)buffer;
    libusb_status_t ret = LIBUSB_ERROR_NO_MEM;

    copy_config_desc(config, (const usb_config_desc_t *)buffer);
    config->interface = calloc(config->bNumInterfaces, sizeof(libusb_interface_t));
    LIBUSB_GOTO_ON_FALSE(config->interface);
    // set pointers to extra data to be used later for class/vendor specific descriptor
    set_extra_data(&extra, &config->extra, &config->extra_length, buffer + LIBUSB_DT_CONFIG_SIZE);
    const usb_intf_desc_t *ifc_desc = (const usb_intf_desc_t *)buffer;

    for (int i = 0; i < config->bNumInterfaces; i++) {
        ifc_desc = next_interface_desc(ifc_desc, config->wTotalLength, &offset);
        // Copy any unknown descriptors into a storage area for drivers to later parse
        LIBUSB_GOTO_ON_ERROR( add_extra_data(&extra, ifc_desc) );

        libusb_interface_t *interface = &config->interface[i];
        // Obtain number of alternate interfaces to given interface number
        int alt_interfaces = usb_parse_interface_number_of_alternate(config_start, ifc_desc->bInterfaceNumber) + 1;
        interface->altsetting = calloc(alt_interfaces, sizeof(libusb_interface_descriptor_t));
        LIBUSB_GOTO_ON_FALSE(interface->altsetting);
        interface->num_altsetting = alt_interfaces;

        for (int alt = 0; alt < alt_interfaces; alt++) {
            libusb_interface_descriptor_t *altsetting = &interface->altsetting[alt];
            copy_interface_desc(altsetting, ifc_desc);
            set_extra_data(&extra, &altsetting->extra, &altsetting->extra_length, ((uint8_t *)ifc_desc) + LIBUSB_DT_INTERFACE_SIZE);
            uint8_t endpoints = ifc_desc->bNumEndpoints;

            altsetting->endpoint = calloc(altsetting->bNumEndpoints, sizeof(libusb_endpoint_descriptor_t));
            LIBUSB_GOTO_ON_FALSE(interface->altsetting);

            for (int ep = 0; ep < endpoints; ep++) {
                ep_desc = usb_parse_endpoint_descriptor_by_index(ifc_desc, ep, config->wTotalLength, &offset);
                ifc_desc = (const usb_intf_desc_t *)ep_desc;
                libusb_endpoint_descriptor_t *endpoint = &altsetting->endpoint[ep];
                copy_endpoint_desc(endpoint, ep_desc);
                LIBUSB_GOTO_ON_ERROR( add_extra_data(&extra, ep_desc) );
                set_extra_data(&extra, &endpoint->extra, &endpoint->extra_length, ((uint8_t *)ep_desc) + ep_desc->bLength);
            }
            if (alt + 1 < alt_interfaces) {
                // go over next alternate interface
                ifc_desc = next_interface_desc(ifc_desc, config->wTotalLength, &offset);
                LIBUSB_GOTO_ON_ERROR( add_extra_data(&extra, ifc_desc) );
                extra.begin = ((uint8_t *)ifc_desc) + LIBUSB_DT_INTERFACE_SIZE;
            }
        }
    }
    // Save any remaining descriptors to extra data
    LIBUSB_GOTO_ON_ERROR( add_extra_data(&extra, &buffer[config->wTotalLength]) );

    return LIBUSB_SUCCESS;

cleanup:
    clear_config_descriptor(config);
    return ret;
}

int raw_desc_to_libusb_config(const uint8_t *buf, int size, struct libusb_config_descriptor **config_desc)
{
    libusb_config_descriptor_t *config = calloc(1, sizeof(*config));

    if (!config) {
        return LIBUSB_ERROR_NO_MEM;
    }

    int r = parse_configuration(config, buf, size);
    if (r < 0) {
        ESP_LOGE(TAG, "parse_configuration failed with error %d", r);
        free(config);
        return r;
    }

    *config_desc = config;
    return LIBUSB_SUCCESS;
}

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bFirstInterface;
    uint8_t bInterfaceCount;
    uint8_t bFunctionClass;
    uint8_t bFunctionSubClass;
    uint8_t bFunctionProtocol;
    uint8_t iFunction;
} USB_DESC_ATTR ifc_assoc_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint16_t bcdUVC;
    uint16_t wTotalLength;
    uint32_t dwClockFrequency;
    uint8_t  bFunctionProtocol;
    uint8_t  bInCollection;
    uint8_t  baInterfaceNr;
} USB_DESC_ATTR vc_interface_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bNumFormats;
    uint16_t wTotalLength;
    uint8_t  bEndpointAddress;
    uint8_t  bFunctionProtocol;
    uint8_t  bmInfo;
    uint8_t  bTerminalLink;
    uint8_t  bStillCaptureMethod;
    uint8_t  bTriggerSupport;
    uint8_t  bTriggerUsage;
    uint8_t  bControlSize;
    uint8_t  bmaControls;

} USB_DESC_ATTR vs_interface_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bTerminalID;
    uint16_t wTerminalType;
    uint8_t  bAssocTerminal;
    uint8_t  iTerminal;
    uint16_t wObjectiveFocalLengthMin;
    uint16_t wObjectiveFocalLengthMax;
    uint16_t wOcularFocalLength;
    uint8_t  bControlSize;
    uint16_t bmControls;
} USB_DESC_ATTR input_terminal_camera_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bTerminalID;
    uint16_t wTerminalType;
    uint8_t  bAssocTerminal;
    uint8_t  iTerminal;
} USB_DESC_ATTR input_terminal_composite_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bTerminalID;
    uint16_t wTerminalType;
    uint8_t  bAssocTerminal;
    uint8_t  iTerminal;
    uint8_t  bControlSize;
    uint8_t  bmControls;
    uint8_t  bTransportModeSize;
    uint8_t  bmTransportModes[5];
} USB_DESC_ATTR input_terminal_media_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bTerminalID;
    uint16_t wTerminalType;
    uint8_t  bAssocTerminal;
    uint8_t  bSourceID;
    uint8_t  iTerminal;
} USB_DESC_ATTR output_terminal_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bUnitID;
    uint8_t  bNrInPins;
    uint8_t  baSourceID1;
    uint8_t  baSourceID2;
    uint8_t  iSelector;
} USB_DESC_ATTR selector_unit_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bUnitID;
    uint8_t  bSourceID;
    uint16_t wMaxMultiplier;
    uint8_t  bControlSize;
    uint16_t bmControls;
    uint8_t  iProcessing;
    uint8_t  bmVideoStandards;
} USB_DESC_ATTR processing_unit_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint16_t wMaxTransferSize;
} USB_DESC_ATTR class_specific_endpoint_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bFormatIndex;
    uint8_t  bNumFrameDescriptors;
    uint8_t  bmFlags;
    uint8_t  bDefaultFrameIndex;
    uint8_t  bAspectRatioX;
    uint8_t  bAspectRatioY;
    uint8_t  bmInterlaceFlags;
    uint8_t  bCopyProtect;
} USB_DESC_ATTR vs_format_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bFormatIndex;
    uint8_t  bmCapabilities;
    uint16_t wWidth;
    uint16_t wHeigh;
    uint32_t dwMinBitRate;
    uint32_t dwMaxBitRate;
    uint32_t dwMaxVideoFrameBufSize;
    uint32_t dwDefaultFrameInterval;
    uint8_t  bFrameIntervalType;
    union {
        uint32_t dwFrameInterval[16];
        struct {
            uint32_t dwMinFrameInterval;
            uint32_t dwMaxFrameInterval;
            uint32_t dwFrameIntervalStep;
        };
    };
} USB_DESC_ATTR vs_frame_desc_t;

// Helper struct
typedef struct {
    uint16_t wWidth;
    uint16_t wHeight;
} USB_DESC_ATTR WidthHeight_t;

// Helper struct
typedef struct {
    uint8_t  bNumCompressionPtn;
    uint8_t  bCompression;
} USB_DESC_ATTR Compression_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bEndpointAddress;
    uint8_t  bNumImageSizePatterns;
    uint16_t wWidth;
    uint16_t wHeight;
    uint8_t  bNumCompressionPtn;
    uint8_t  bCompression;
} USB_DESC_ATTR still_image_frame_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bColorPrimaries;
    uint8_t  bTransferCharacteristics;
    uint8_t  bMatrixCoefficients;
} USB_DESC_ATTR color_format_desc_t;

#define TERMINAL_INPUT_CAMERA_TYPE      0x0201
#define TERMINAL_INPUT_COMPOSITE_TYPE   0x0401
#define ITT_MEDIA_TRANSPORT_INPUT       0x0202

#define CC_VIDEO 0x0E

#define USB_DESC_ASC_SIZE 8

typedef enum {
    CONFIG_DESC = 0x02,
    STRING_DESC = 0x03,
    INTERFACE_DESC = 0x04,
    ENDPOINT_DESC = 0x05,
    INTERFACE_ASSOC_DESC = 0x0B,
    CS_INTERFACE_DESC = 0x24,
    CS_ENDPOINT_DESC = 0x25,
} descriptor_types_t;

typedef enum {
    VC_HEADER = 0x01,
    VC_INPUT_TERMINAL = 0x02,
    VC_OUTPUT_TERMINAL = 0x03,
    VC_SELECTOR_UNIT = 0x04,
    VC_PROCESSING_UNIT = 0x05,
    VS_FORMAT_MJPEG = 0x06,
    VS_FRAME_MJPEG = 0x07,
    VS_STILL_FRAME = 0x03,
    VS_COLORFORMAT = 0x0D,
} descriptor_subtypes_t;

typedef enum {
    SC_VIDEOCONTROL = 1,
    SC_VIDEOSTREAMING = 2,
} interface_sub_class_t;

static interface_sub_class_t interface_sub_class = SC_VIDEOCONTROL;

static void print_cs_endpoint_desc(const uint8_t *buff)
{
    class_specific_endpoint_desc_t *class_desc = (class_specific_endpoint_desc_t *)buff;
    printf("\t\t*** Class-specific Interrupt Endpoint Descriptor ***\n");
    printf("\t\tbLength 0x%x\n", class_desc->bLength);
    printf("\t\tbDescriptorType 0x%x\n", class_desc->bDescriptorType);
    printf("\t\tbDescriptorSubType %d\n", class_desc->bDescriptorSubType);
    printf("\t\twMaxTransferSize %d\n", class_desc->wMaxTransferSize);
}

static void print_interface_assoc_desc(const uint8_t *buff)
{
    const ifc_assoc_desc_t *asc_desc = (const ifc_assoc_desc_t *) buff;
    printf("\t*** Interface Association Descriptor ***\n");
    printf("\tbLength 0x%x\n", asc_desc->bLength);
    printf("\tbDescriptorType 0x%x\n", asc_desc->bDescriptorType);
    printf("\tbInterfaceCount %u\n", asc_desc->bInterfaceCount);
    printf("\tbFirstInterface %d\n", asc_desc->bFirstInterface);
    printf("\tbFunctionClass %d\n", asc_desc->bFunctionClass);
    printf("\tbFunctionSubClass %d\n", asc_desc->bFunctionSubClass);
    printf("\tbFunctionProtocol %d\n", asc_desc->bFunctionProtocol);
    printf("\tiFunction 0x%x\n", asc_desc->iFunction);
}


static void print_class_header_desc(const uint8_t *buff)
{
    if (interface_sub_class == SC_VIDEOCONTROL) {
        const vc_interface_desc_t *desc = (const vc_interface_desc_t *) buff;
        printf("\t*** Class-specific VC Interface Descriptor ***\n");
        printf("\tbLength 0x%x\n", desc->bLength);
        printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
        printf("\tbDescriptorSubType %u\n", desc->bDescriptorSubType);
        printf("\tbcdUVC %x\n", desc->bcdUVC);
        printf("\twTotalLength %u\n", desc->wTotalLength);
        printf("\tdwClockFrequency %lu\n", desc->dwClockFrequency);
        printf("\tbFunctionProtocol %u\n", desc->bFunctionProtocol);
        printf("\tbInCollection %u\n", desc->bInCollection);
        printf("\tbaInterfaceNr %u\n", desc->baInterfaceNr);
    } else if (interface_sub_class == SC_VIDEOSTREAMING) {
        const vs_interface_desc_t *desc = (const vs_interface_desc_t *) buff;
        printf("\t*** Class-specific VS Interface Descriptor ***\n");
        printf("\tbLength 0x%x\n", desc->bLength);
        printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
        printf("\tbDescriptorSubType %u\n", desc->bDescriptorSubType);
        printf("\tbNumFormats %x\n", desc->bNumFormats);
        printf("\twTotalLength %u\n", desc->wTotalLength);
        printf("\tbEndpointAddress %u\n", desc->bEndpointAddress);
        printf("\tbFunctionProtocol %u\n", desc->bFunctionProtocol);
        printf("\tbmInfo 0x%x\n", desc->bmInfo);
        printf("\tbTerminalLink %u\n", desc->bTerminalLink);
        printf("\tbStillCaptureMethod %u\n", desc->bStillCaptureMethod);
        printf("\tbTriggerSupport %u\n", desc->bTriggerSupport);
        printf("\tbTriggerUsage %u\n", desc->bTriggerUsage);
        printf("\tbControlSize %u\n", desc->bControlSize);
        printf("\tbmaControls 0x%x\n", desc->bmaControls);
    }
}

static void print_vc_input_terminal_desc(const uint8_t *buff)
{
    const input_terminal_camera_desc_t *desc = (const input_terminal_camera_desc_t *) buff;

    const char *type = NULL;

    switch (desc->wTerminalType) {
    case TERMINAL_INPUT_CAMERA_TYPE: type = "Camera"; break;
    case TERMINAL_INPUT_COMPOSITE_TYPE: type = "Composite"; break;
    case ITT_MEDIA_TRANSPORT_INPUT: type = "Media"; break;
    default: printf("!!!!! Unknown Input terminal descriptor !!!!!\n"); return;

    }

    printf("\t*** Input Terminal Descriptor (%s) ***\n", type);
    printf("\tbLength 0x%x\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubType %u\n", desc->bDescriptorSubType);
    printf("\tbTerminalID %x\n", desc->bTerminalID);
    printf("\twTerminalType %x\n", desc->wTerminalType);
    printf("\tbAssocTerminal %u\n", desc->bAssocTerminal);
    printf("\tiTerminal %u\n", desc->iTerminal);

    if (desc->wTerminalType == TERMINAL_INPUT_COMPOSITE_TYPE) {
        return;
    } else if (desc->wTerminalType == TERMINAL_INPUT_CAMERA_TYPE) {
        printf("\twObjectiveFocalLengthMin %u\n", desc->wObjectiveFocalLengthMin);
        printf("\twObjectiveFocalLengthMax %u\n", desc->wObjectiveFocalLengthMax);
        printf("\twOcularFocalLength %u\n", desc->wOcularFocalLength);
        printf("\tbControlSize %u\n", desc->bControlSize);
        printf("\tbmControls 0x%x\n", desc->bmControls);
    } else if (desc->wTerminalType == ITT_MEDIA_TRANSPORT_INPUT) {
        const input_terminal_media_desc_t *desc = (const input_terminal_media_desc_t *) buff;
        printf("\tbControlSize %u\n", desc->bControlSize);
        printf("\tbmControls 0x%x\n", desc->bmControls);
        printf("\tbTransportModeSize %u\n", desc->bTransportModeSize);
        printf("\tbmTransportModes 0x%x 0x%x 0x%x 0x%x 0x%x\n",
               desc->bmTransportModes[0],
               desc->bmTransportModes[1],
               desc->bmTransportModes[2],
               desc->bmTransportModes[3],
               desc->bmTransportModes[4]);
    }
}

static void print_vc_output_terminal_desc(const uint8_t *buff)
{
    const output_terminal_desc_t *desc = (const output_terminal_desc_t *) buff;
    printf("\t*** Output Terminal Descriptor ***\n");
    printf("\tbLength 0x%x\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubType %u\n", desc->bDescriptorSubType);
    printf("\tbTerminalID %u\n", desc->bTerminalID);
    printf("\twTerminalType %x\n", desc->wTerminalType);
    printf("\tbAssocTerminal %u\n", desc->bAssocTerminal);
    printf("\tbSourceID %u\n", desc->bSourceID);
    printf("\tiTerminal %u\n", desc->iTerminal);
}

static void print_vc_selector_unit_desc(const uint8_t *buff)
{
    const selector_unit_desc_t *desc = (const selector_unit_desc_t *) buff;
    printf("\t*** Selector Unit Descriptor ***\n");
    printf("\tbLength 0x%x\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubType %u\n", desc->bDescriptorSubType);
    printf("\tbUnitID %u\n", desc->bUnitID);
    printf("\tbNrInPins %u\n", desc->bNrInPins);
    printf("\tbaSourceID1 %u\n", desc->baSourceID1);
    printf("\tbaSourceID2 %u\n", desc->baSourceID2);
    printf("\tiSelector %u\n", desc->iSelector);
}

static void print_vc_processing_unit_desc(const uint8_t *buff)
{
    const processing_unit_desc_t *desc = (const processing_unit_desc_t *) buff;
    printf("\t*** Processing Unit Descriptor ***\n");
    printf("\tbLength 0x%x\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubType %u\n", desc->bDescriptorSubType);
    printf("\tbUnitID %u\n", desc->bUnitID);
    printf("\tbSourceID %u\n", desc->bSourceID);
    printf("\twMaxMultiplier %u\n", desc->wMaxMultiplier);
    printf("\tbControlSize %u\n", desc->bControlSize);
    printf("\tbmControls 0x%x\n", desc->bmControls);
    printf("\tiProcessing %u\n", desc->iProcessing);
    printf("\tbmVideoStandards 0x%x\n", desc->bmVideoStandards);
}

static void print_vs_format_mjpeg_desc(const uint8_t *buff)
{
    const vs_format_desc_t *desc = (const vs_format_desc_t *) buff;
    printf("\t*** VS Format Descriptor ***\n");
    printf("\tbLength 0x%x\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubType 0x%x\n", desc->bDescriptorSubType);
    printf("\tbFormatIndex 0x%x\n", desc->bFormatIndex);
    printf("\tbNumFrameDescriptors %u\n", desc->bNumFrameDescriptors);
    printf("\tbmFlags 0x%x\n", desc->bmFlags);
    printf("\tbDefaultFrameIndex %u\n", desc->bDefaultFrameIndex);
    printf("\tbAspectRatioX %u\n", desc->bAspectRatioX);
    printf("\tbAspectRatioY %u\n", desc->bAspectRatioY);
    printf("\tbmInterlaceFlags 0x%x\n", desc->bmInterlaceFlags);
    printf("\tbCopyProtect %u\n", desc->bCopyProtect);
}

static void print_vs_frame_mjpeg_desc(const uint8_t *buff)
{
    // Copy to local buffer due to potential misalignment issues.
    uint32_t raw_desc[25];
    uint32_t desc_size = ((const vs_frame_desc_t *)buff)->bLength;
    memcpy(raw_desc, buff, desc_size);

    const vs_frame_desc_t *desc = (const vs_frame_desc_t *) raw_desc;
    printf("\t*** VS Frame Descriptor ***\n");
    printf("\tbLength 0x%x\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubType 0x%x\n", desc->bDescriptorSubType);
    printf("\tbFormatIndex 0x%x\n", desc->bFormatIndex);
    printf("\tbmCapabilities 0x%x\n", desc->bmCapabilities);
    printf("\twWidth %u\n", desc->wWidth);
    printf("\twHeigh %u\n", desc->wHeigh);
    printf("\tdwMinBitRate %lu\n", desc->dwMinBitRate);
    printf("\tdwMaxBitRate %lu\n", desc->dwMaxBitRate);
    printf("\tdwMaxVideoFrameBufSize %lu\n", desc->dwMaxVideoFrameBufSize);
    printf("\tdwDefaultFrameInterval %lu\n", desc->dwDefaultFrameInterval);
    printf("\tbFrameIntervalType %u\n", desc->bFrameIntervalType);

    if (desc->bFrameIntervalType == 0) {
        // Continuous Frame Intervals
        printf("\tdwMinFrameInterval %lu\n",  desc->dwMinFrameInterval);
        printf("\tdwMaxFrameInterval %lu\n",  desc->dwMaxFrameInterval);
        printf("\tdwFrameIntervalStep %lu\n", desc->dwFrameIntervalStep);
    } else {
        // Discrete Frame Intervals
        size_t max_intervals = sizeof(desc->dwFrameInterval) / sizeof(desc->dwFrameInterval[0]);
        size_t num_of_intervals = MIN((desc->bLength - 26) / 4, max_intervals);
        for (int i = 0; i < num_of_intervals; ++i) {
            printf("\tFrameInterval[%d] %lu\n", i, desc->dwFrameInterval[i]);
        }
    }
}

static void print_vs_still_frame_desc(const uint8_t *buff)
{
    const still_image_frame_desc_t *desc = (const still_image_frame_desc_t *) buff;
    printf("\t*** VS Still Format Descriptor ***\n");
    printf("\tbLength 0x%x\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubType 0x%x\n", desc->bDescriptorSubType);
    printf("\tbEndpointAddress 0x%x\n", desc->bEndpointAddress);
    printf("\tbNumImageSizePatterns 0x%x\n", desc->bNumImageSizePatterns);

    WidthHeight_t *wh = (WidthHeight_t *)&desc->wWidth;
    for (int i = 0; i < desc->bNumImageSizePatterns; ++i, wh++) {
        printf("\t[%d]: wWidth: %u, wHeight: %u\n", i, wh->wWidth, wh->wHeight);
    }

    Compression_t *c = (Compression_t *)wh;
    printf("\tbNumCompressionPtn %u\n", c->bNumCompressionPtn);
    printf("\tbCompression %u\n", c->bCompression);
}

static void print_vs_color_format_desc(const uint8_t *buff)
{
    const color_format_desc_t *desc = (const color_format_desc_t *) buff;
    printf("\t*** VS Color Format Descriptor ***\n");
    printf("\tbLength 0x%x\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubType 0x%x\n", desc->bDescriptorSubType);
    printf("\tbColorPrimaries 0x%x\n", desc->bColorPrimaries);
    printf("\tbTransferCharacteristics %u\n", desc->bTransferCharacteristics);
    printf("\tbMatrixCoefficients 0x%x\n", desc->bMatrixCoefficients);
}

static void unknown_desc(const desc_header_t *header)
{
    printf(" *** Unknown Descriptor Length: %d Type: %d Subtype: %d ***\n",
           header->bLength, header->bDescriptorType, header->bDescriptorSubtype);
}

static void print_class_specific_desc(const uint8_t *buff)
{
    desc_header_t *header = (desc_header_t *)buff;

    switch (header->bDescriptorSubtype) {
    case VC_HEADER:
        print_class_header_desc(buff);
        break;
    case VC_INPUT_TERMINAL:
        print_vc_input_terminal_desc(buff);
        break;
    case VC_SELECTOR_UNIT:
        print_vc_selector_unit_desc(buff);
        break;
    case VC_PROCESSING_UNIT:
        print_vc_processing_unit_desc(buff);
        break;
    case VS_FORMAT_MJPEG:
        if (interface_sub_class == SC_VIDEOCONTROL) {
            printf("\t*** Extension Unit Descriptor unsupported, skipping... ***\n");;
            return;
        }
        print_vs_format_mjpeg_desc(buff);
        break;
    case VS_FRAME_MJPEG:
        print_vs_frame_mjpeg_desc(buff);
        break;
    case VS_COLORFORMAT:
        print_vs_color_format_desc(buff);
        break;
    case VC_OUTPUT_TERMINAL: // same as VS_STILL_FRAME
        if (interface_sub_class == SC_VIDEOCONTROL) {
            print_vc_output_terminal_desc(buff);
        } else {
            print_vs_still_frame_desc(buff);
        }
        break;
    default:
        unknown_desc(header);
        break;
    }
}

void print_usb_class_descriptors(const usb_standard_desc_t *desc)
{
    const uint8_t *buff = (uint8_t *)desc;
    desc_header_t *header = (desc_header_t *)desc;

    switch (header->bDescriptorType) {
    case INTERFACE_ASSOC_DESC:
        print_interface_assoc_desc(buff);
        break;
    case CS_INTERFACE_DESC:
        print_class_specific_desc(buff);
        break;
    case CS_ENDPOINT_DESC:
        print_cs_endpoint_desc(buff);
        break;
    default:
        unknown_desc(header);
        break;
    }
}
