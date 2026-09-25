/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_check.h"
#include "nand.h"
#include "spi_nand_oper.h"
#include "nand_flash_devices.h"
#include "nand_xtx_ecc_decode.h"

static const char *TAG = "nand_xtx";

/* ECC status decoder installed as dev->ecc_status_decoder. The bit decoding is in
 * nand_xtx_ecc_decode.c so host tests can exercise it. */
static esp_err_t xtx_ecc_decode(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    (void)dev;
    *out = nand_xtx_ecc_decode(status_c0);
    return ESP_OK;
}

esp_err_t spi_nand_xtx_init(spi_nand_flash_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    uint8_t device_id = 0;
    ESP_RETURN_ON_ERROR(spi_nand_read_device_id(dev, &device_id, sizeof(device_id)), TAG, "%s, Failed to get the device ID %d", __func__, ret);
    dev->device_info.device_id = device_id;
    snprintf(dev->device_info.chip_name, sizeof(dev->device_info.chip_name),
             "xtx-0x%02" PRIx8, device_id);
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);

    dev->chip.has_quad_enable_bit = 1;
    dev->chip.quad_enable_bit_pos = 0;
    dev->chip.erase_block_delay_us = 3500;
    dev->chip.program_page_delay_us = 650;
    dev->chip.read_page_delay_us = 50;
    dev->ecc_status_decoder = xtx_ecc_decode;
    switch (device_id) {
    case XTX_DI_37: // XT26G08D - 8 bits/528B ECC strength (ECC always on)
        dev->chip.num_blocks = 4096;
        dev->chip.log2_ppb = 6;        // 64 pages per block
        dev->chip.log2_page_size = 12; // 4096 bytes per page
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
