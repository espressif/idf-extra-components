/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ONFI parameter page CRC-16 over @p data[0..length-1].
 * Polynomial 0x8005, initial value 0x4F4E (ONFI 1.0).
 */
uint16_t nand_onfi_param_page_crc16(const uint8_t *data, size_t length);

/** Validate bytes 0-253 against CRC stored in bytes 254-255 (little-endian). */
bool nand_onfi_param_page_crc_valid(const uint8_t *page_data, size_t page_size);

#ifdef __cplusplus
}
#endif
