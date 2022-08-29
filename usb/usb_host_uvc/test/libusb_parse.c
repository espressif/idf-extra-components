/*
 * SPDX-FileCopyrightText: 2007 Daniel Drake <dsd@gentoo.org>
 * SPDX-FileCopyrightText: 2001 Johannes Erdfelt <johannes@erdfelt.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * SPDX-FileContributor: 2022 Espressif Systems (Shanghai) CO LTD
 */

/*
 * USB descriptor handling functions for libusb
 * Copyright © 2007 Daniel Drake <dsd@gentoo.org>
 * Copyright © 2001 Johannes Erdfelt <johannes@erdfelt.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <endian.h>
#include <sys/param.h>
#include "libusb.h"

#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"

#define DESC_HEADER_LENGTH  2
#define USB_MAXENDPOINTS    32
#define USB_MAXINTERFACES   32
#define USB_MAXCONFIG       8

const char *TAG = "LIBUSB_PARSE";

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
} desc_header_t;


static void parse_descriptor(const void *source, const char *descriptor, void *dest)
{
    const uint8_t *sp = source;
    uint8_t *dp = dest;
    char field_type;

    while (*descriptor) {
        field_type = *descriptor++;
        switch (field_type) {
        case 'b':   /* 8-bit byte */
            *dp++ = *sp++;
            break;
        case 'w':   /* 16-bit word, convert from little endian to CPU */
            dp += ((uintptr_t)dp & 1);  /* Align to 16-bit word boundary */

            // *((uint16_t *)dp) = le16toh(*((uint16_t *)sp));
            *((uint16_t *)dp) = le16toh((uint16_t)sp[0] | sp[1] << 8);
            sp += 2;
            dp += 2;
            break;
        case 'd':   /* 32-bit word, convert from little endian to CPU */
            dp += 4 - ((uintptr_t)dp & 3);  /* Align to 32-bit word boundary */

            *((uint32_t *)dp) = le32toh(((uint32_t)sp[0] | sp[1] << 8 | sp[2] << 16 | sp[3] << 24));
            sp += 4;
            dp += 4;
            break;
        case 'u':   /* 16 byte UUID */
            memcpy(dp, sp, 16);
            sp += 16;
            dp += 16;
            break;
        }
    }
}

static void clear_endpoint(struct libusb_endpoint_descriptor *endpoint)
{
    free((void *)endpoint->extra);
}

static int parse_endpoint(struct libusb_endpoint_descriptor *endpoint, const uint8_t *buffer, int size)
{
    const desc_header_t *header;
    const uint8_t *begin;
    void *extra;
    int parsed = 0;
    int len;

    if (size < DESC_HEADER_LENGTH) {
        ESP_LOGE(TAG, "short endpoint descriptor read %d/%d",
                 size, DESC_HEADER_LENGTH);
        return LIBUSB_ERROR_IO;
    }

    header = (const desc_header_t *)buffer;
    if (header->bDescriptorType != LIBUSB_DT_ENDPOINT) {
        ESP_LOGE(TAG, "unexpected descriptor 0x%x (expected 0x%x)",
                 header->bDescriptorType, LIBUSB_DT_ENDPOINT);
        return parsed;
    } else if (header->bLength < LIBUSB_DT_ENDPOINT_SIZE) {
        ESP_LOGE(TAG, "invalid endpoint bLength (%u)", header->bLength);
        return LIBUSB_ERROR_IO;
    } else if (header->bLength > size) {
        ESP_LOGW(TAG, "short endpoint descriptor read %d/%u",
                 size, header->bLength);
        return parsed;
    }

    if (header->bLength >= LIBUSB_DT_ENDPOINT_AUDIO_SIZE) {
        parse_descriptor(buffer, "bbbbwbbb", endpoint);
    } else {
        parse_descriptor(buffer, "bbbbwb", endpoint);
    }

    buffer += header->bLength;
    size -= header->bLength;
    parsed += header->bLength;

    /* Skip over the rest of the Class Specific or Vendor Specific */
    /*  descriptors */
    begin = buffer;
    while (size >= DESC_HEADER_LENGTH) {
        header = (const desc_header_t *)buffer;
        if (header->bLength < DESC_HEADER_LENGTH) {
            ESP_LOGE(TAG, "invalid extra ep desc len (%u)",
                     header->bLength);
            return LIBUSB_ERROR_IO;
        } else if (header->bLength > size) {
            ESP_LOGW(TAG, "short extra ep desc read %d/%u",
                     size, header->bLength);
            return parsed;
        }

        /* If we find another "proper" descriptor then we're done  */
        if (header->bDescriptorType == LIBUSB_DT_ENDPOINT ||
                header->bDescriptorType == LIBUSB_DT_INTERFACE ||
                header->bDescriptorType == LIBUSB_DT_CONFIG ||
                header->bDescriptorType == LIBUSB_DT_DEVICE) {
            break;
        }

        ESP_LOGD(TAG, "skipping descriptor 0x%x", header->bDescriptorType);
        buffer += header->bLength;
        size -= header->bLength;
        parsed += header->bLength;
    }

    /* Copy any unknown descriptors into a storage area for drivers */
    /*  to later parse */
    len = (int)(buffer - begin);
    if (len <= 0) {
        return parsed;
    }

    extra = malloc((size_t)len);
    if (!extra) {
        return LIBUSB_ERROR_NO_MEM;
    }

    memcpy(extra, begin, len);
    endpoint->extra = extra;
    endpoint->extra_length = len;

    return parsed;
}

static void clear_interface(struct libusb_interface *usb_interface)
{
    int i;

    if (usb_interface->altsetting) {
        for (i = 0; i < usb_interface->num_altsetting; i++) {
            struct libusb_interface_descriptor *ifp =
                (struct libusb_interface_descriptor *)
                usb_interface->altsetting + i;

            free((void *)ifp->extra);
            if (ifp->endpoint) {
                uint8_t j;

                for (j = 0; j < ifp->bNumEndpoints; j++)
                    clear_endpoint((struct libusb_endpoint_descriptor *)
                                   ifp->endpoint + j);
            }
            free((void *)ifp->endpoint);
        }
    }
    free((void *)usb_interface->altsetting);
    usb_interface->altsetting = NULL;
}

static int parse_interface(struct libusb_interface *usb_interface, const uint8_t *buffer, int size)
{
    int len;
    int r;
    int parsed = 0;
    int interface_number = -1;
    const desc_header_t *header;
    const usb_intf_desc_t *if_desc;
    struct libusb_interface_descriptor *ifp;
    const uint8_t *begin;

    while (size >= LIBUSB_DT_INTERFACE_SIZE) {
        struct libusb_interface_descriptor *altsetting;

        altsetting = realloc((void *)usb_interface->altsetting,
                             sizeof(*altsetting) * (size_t)(usb_interface->num_altsetting + 1));
        if (!altsetting) {
            r = LIBUSB_ERROR_NO_MEM;
            goto err;
        }
        usb_interface->altsetting = altsetting;

        ifp = altsetting + usb_interface->num_altsetting;
        parse_descriptor(buffer, "bbbbbbbbb", ifp);
        if (ifp->bDescriptorType != LIBUSB_DT_INTERFACE) {
            ESP_LOGE(TAG, "unexpected descriptor 0x%x (expected 0x%x)",
                     ifp->bDescriptorType, LIBUSB_DT_INTERFACE);
            return parsed;
        } else if (ifp->bLength < LIBUSB_DT_INTERFACE_SIZE) {
            ESP_LOGE(TAG, "invalid interface bLength (%u)",
                     ifp->bLength);
            r = LIBUSB_ERROR_IO;
            goto err;
        } else if (ifp->bLength > size) {
            ESP_LOGW(TAG, "short intf descriptor read %d/%u",
                     size, ifp->bLength);
            return parsed;
        } else if (ifp->bNumEndpoints > USB_MAXENDPOINTS) {
            ESP_LOGE(TAG, "too many endpoints (%u)", ifp->bNumEndpoints);
            r = LIBUSB_ERROR_IO;
            goto err;
        }

        usb_interface->num_altsetting++;
        ifp->extra = NULL;
        ifp->extra_length = 0;
        ifp->endpoint = NULL;

        if (interface_number == -1) {
            interface_number = ifp->bInterfaceNumber;
        }

        /* Skip over the interface */
        buffer += ifp->bLength;
        parsed += ifp->bLength;
        size -= ifp->bLength;

        begin = buffer;

        /* Skip over any interface, class or vendor descriptors */
        while (size >= DESC_HEADER_LENGTH) {
            header = (const desc_header_t *)buffer;
            if (header->bLength < DESC_HEADER_LENGTH) {
                ESP_LOGE(TAG, "invalid extra intf desc len (%u)", header->bLength);
                r = LIBUSB_ERROR_IO;
                goto err;
            } else if (header->bLength > size) {
                ESP_LOGW(TAG, "short extra intf desc read %d/%u", size, header->bLength);
                return parsed;
            }

            /* If we find another "proper" descriptor then we're done */
            if (header->bDescriptorType == LIBUSB_DT_INTERFACE ||
                    header->bDescriptorType == LIBUSB_DT_ENDPOINT ||
                    header->bDescriptorType == LIBUSB_DT_CONFIG ||
                    header->bDescriptorType == LIBUSB_DT_DEVICE) {
                break;
            }

            buffer += header->bLength;
            parsed += header->bLength;
            size -= header->bLength;
        }

        /* Copy any unknown descriptors into a storage area for */
        /*  drivers to later parse */
        len = (int)(buffer - begin);
        if (len > 0) {
            void *extra = malloc((size_t)len);

            if (!extra) {
                r = LIBUSB_ERROR_NO_MEM;
                goto err;
            }

            memcpy(extra, begin, len);
            ifp->extra = extra;
            ifp->extra_length = len;
        }

        if (ifp->bNumEndpoints > 0) {
            struct libusb_endpoint_descriptor *endpoint;
            uint8_t i;

            endpoint = calloc(ifp->bNumEndpoints, sizeof(*endpoint));
            if (!endpoint) {
                r = LIBUSB_ERROR_NO_MEM;
                goto err;
            }

            ifp->endpoint = endpoint;
            for (i = 0; i < ifp->bNumEndpoints; i++) {
                r = parse_endpoint(endpoint + i, buffer, size);
                if (r < 0) {
                    goto err;
                }
                if (r == 0) {
                    ifp->bNumEndpoints = i;
                    break;
                }

                buffer += r;
                parsed += r;
                size -= r;
            }
        }

        /* We check to see if it's an alternate to this one */
        if_desc = (const usb_intf_desc_t *)buffer;
        if (size < LIBUSB_DT_INTERFACE_SIZE ||
                if_desc->bDescriptorType != LIBUSB_DT_INTERFACE ||
                if_desc->bInterfaceNumber != interface_number) {
            return parsed;
        }
    }

    return parsed;
err:
    clear_interface(usb_interface);
    return r;
}

void libusb_clear_config_descriptor(struct libusb_config_descriptor *config)
{
    uint8_t i;

    if (config->interface) {
        for (i = 0; i < config->bNumInterfaces; i++) {
            clear_interface((struct libusb_interface *) config->interface + i);
        }
    }
    free((void *)config->interface);
    free((void *)config->extra);
}

int libusb_parse_configuration(struct libusb_config_descriptor *config, const uint8_t *buffer, int size)
{
    uint8_t i;
    int r;
    const desc_header_t *header;
    struct libusb_interface *usb_interface;

    if (size < LIBUSB_DT_CONFIG_SIZE) {
        ESP_LOGE(TAG, "short config descriptor read %d/%d",
                 size, LIBUSB_DT_CONFIG_SIZE);
        return LIBUSB_ERROR_IO;
    }

    parse_descriptor(buffer, "bbwbbbbb", config);
    if (config->bDescriptorType != LIBUSB_DT_CONFIG) {
        ESP_LOGE(TAG, "unexpected descriptor 0x%x (expected 0x%x)",
                 config->bDescriptorType, LIBUSB_DT_CONFIG);
        return LIBUSB_ERROR_IO;
    } else if (config->bLength < LIBUSB_DT_CONFIG_SIZE) {
        ESP_LOGE(TAG, "invalid config bLength (%u)", config->bLength);
        return LIBUSB_ERROR_IO;
    } else if (config->bLength > size) {
        ESP_LOGE(TAG, "short config descriptor read %d/%u",
                 size, config->bLength);
        return LIBUSB_ERROR_IO;
    } else if (config->bNumInterfaces > USB_MAXINTERFACES) {
        ESP_LOGE(TAG, "too many interfaces (%u)", config->bNumInterfaces);
        return LIBUSB_ERROR_IO;
    }

    usb_interface = calloc(config->bNumInterfaces, sizeof(*usb_interface));
    if (!usb_interface) {
        return LIBUSB_ERROR_NO_MEM;
    }

    config->interface = usb_interface;

    buffer += config->bLength;
    size -= config->bLength;

    for (i = 0; i < config->bNumInterfaces; i++) {
        int len;
        const uint8_t *begin;

        /* Skip over the rest of the Class Specific or Vendor */
        /*  Specific descriptors */
        begin = buffer;
        while (size >= DESC_HEADER_LENGTH) {
            header = (const desc_header_t *)buffer;
            if (header->bLength < DESC_HEADER_LENGTH) {
                ESP_LOGE(TAG, "invalid extra config desc len (%u)", header->bLength);
                r = LIBUSB_ERROR_IO;
                goto err;
            } else if (header->bLength > size) {
                ESP_LOGW(TAG, "short extra config desc read %d/%u", size, header->bLength);
                config->bNumInterfaces = i;
                return size;
            }

            /* If we find another "proper" descriptor then we're done */
            if (header->bDescriptorType == LIBUSB_DT_ENDPOINT ||
                    header->bDescriptorType == LIBUSB_DT_INTERFACE ||
                    header->bDescriptorType == LIBUSB_DT_CONFIG ||
                    header->bDescriptorType == LIBUSB_DT_DEVICE) {
                break;
            }

            ESP_LOGD(TAG, "skipping descriptor 0x%x", header->bDescriptorType);
            buffer += header->bLength;
            size -= header->bLength;
        }

        /* Copy any unknown descriptors into a storage area for */
        /*  drivers to later parse */
        len = (int)(buffer - begin);
        if (len > 0) {
            uint8_t *extra = realloc((void *)config->extra,
                                     (size_t)(config->extra_length + len));

            if (!extra) {
                r = LIBUSB_ERROR_NO_MEM;
                goto err;
            }

            memcpy(extra + config->extra_length, begin, len);
            config->extra = extra;
            config->extra_length += len;
        }

        r = parse_interface(usb_interface + i, buffer, size);
        if (r < 0) {
            goto err;
        }
        if (r == 0) {
            config->bNumInterfaces = i;
            break;
        }

        buffer += r;
        size -= r;
    }

    return size;

err:
    libusb_clear_config_descriptor(config);
    return r;
}
