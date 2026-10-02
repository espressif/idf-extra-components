/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_check.h"
#include "nand.h"
#include "spi_nand_oper.h"
#include "nand_flash_devices.h"
#include "nand_macronix_ecc_decode.h"

static const char *TAG = "nand_macronix";

/* Read ECC Status: 7Ch, one dummy byte, then the ECCSR byte. Not a Get Feature register,
 * so keep it vendor-local. The dummy byte goes out as a 1-byte address, the same shape as
 * spi_nand_read_register(), so it works in both full- and half-duplex modes. */
#define MX_CMD_READ_ECCSR  0x7C

static esp_err_t mx_read_eccsr(spi_nand_flash_device_t *dev, uint8_t *eccsr)
{
    spi_nand_transaction_t t = {
        .command = MX_CMD_READ_ECCSR,
        .address_bytes = 1,
        .address = 0,
        .miso_len = 1,
        .miso_data = eccsr,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    return spi_nand_execute_transaction(dev, &t);
}

/* ECC status decoder installed as dev->ecc_status_decoder. Only the 7Ch read lives here;
 * the bit decoding is in nand_macronix_ecc_decode.c so host tests can exercise it. */
static esp_err_t mx_ecc_decode(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    uint8_t eccsr = 0;

    if (nand_mx_ecc_needs_eccsr(status_c0)) {
        ESP_RETURN_ON_ERROR(mx_read_eccsr(dev, &eccsr), TAG, "failed to read ECC status (7Ch)");
    }
    *out = nand_mx_ecc_decode(status_c0, eccsr);
    return ESP_OK;
}

esp_err_t spi_nand_macronix_init(spi_nand_flash_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    uint8_t device_id = 0;
    ESP_RETURN_ON_ERROR(spi_nand_read_device_id(dev, &device_id, sizeof(device_id)), TAG, "%s, Failed to get the device ID %d", __func__, ret);
    dev->device_info.device_id = device_id;
    snprintf(dev->device_info.chip_name, sizeof(dev->device_info.chip_name),
             "macronix-0x%02" PRIx8, device_id);
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);

    dev->chip.has_quad_enable_bit = 1;
    dev->chip.quad_enable_bit_pos = 0;
    dev->ecc_status_decoder = mx_ecc_decode;
    switch (device_id) {
    case MACRONIX_DI_26: // MX35LF2GE4AD (2Gb) - 8 bits/544B ECC strength
        dev->chip.num_blocks = 2048;
        dev->chip.log2_page_size = 11;  // 2048 bytes
        dev->chip.log2_ppb = 6;         // 64 pages per block
        dev->chip.read_page_delay_us = 10;
        dev->chip.erase_block_delay_us = 6000;
        dev->chip.program_page_delay_us = 400;
        break;
    case MACRONIX_DI_37: // MX35LF4GE4AD (4Gb) - 8 bits/544B ECC strength
        dev->chip.num_blocks = 2048;
        dev->chip.log2_page_size = 12;  // 4096 bytes
        dev->chip.log2_ppb = 6;         // 64 pages per block
        dev->chip.read_page_delay_us = 12;
        dev->chip.erase_block_delay_us = 6000;
        dev->chip.program_page_delay_us = 440;
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
