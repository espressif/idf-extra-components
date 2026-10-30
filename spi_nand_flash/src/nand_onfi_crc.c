/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nand_onfi_crc.h"
#include "nand_onfi_param_page.h"

uint16_t nand_onfi_param_page_crc16(const uint8_t *data, size_t length)
{
    const uint16_t poly = 0x8005;
    uint16_t crc = 0x4F4E;

    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ poly);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

bool nand_onfi_param_page_crc_valid(const uint8_t *page_data, size_t page_size)
{
    if (page_size < NAND_ONFI_PARAM_PAGE_SIZE) {
        return false;
    }
    uint16_t computed = nand_onfi_param_page_crc16(page_data, NAND_ONFI_PARAM_PAGE_SIZE - 2);
    uint16_t stored = (uint16_t)page_data[254] | ((uint16_t)page_data[255] << 8);
    return computed == stored;
}
