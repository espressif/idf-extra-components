/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Converts raw buffer containing config descriptor into libusb_config_descriptor
 *
 * @note Call clear_config_descriptor when config descriptor is no longer needed.
 *
 * @param[in]  buf      buffer containing config descriptor
 * @param[in]  size     size of buffer
 * @param[out] config   pointer to allocated libusb compatible config descriptor
 * @return libusb_error
 */
int raw_desc_to_libusb_config(const uint8_t *buf, int size, struct libusb_config_descriptor **config);

/**
 * @brief Releases memory previously allocated by config raw_desc_to_libusb_config
 *
 * @param[in] config  pointer to allocated config descriptor
 */
void clear_config_descriptor(struct libusb_config_descriptor *config);

/**
 * @brief Prints class specific descriptors
 *
 * @param[in] desc  pointer to usb_standard_desc_t
 */
void print_usb_class_descriptors(const usb_standard_desc_t *desc);

#ifdef __cplusplus
}
#endif
