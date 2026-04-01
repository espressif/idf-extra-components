/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_check.h"
#include "nand.h"
#include "spi_nand_oper.h"
#include "nand_flash_devices.h"

static const char *TAG = "nand_macronix";

esp_err_t spi_nand_macronix_init(spi_nand_flash_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    uint8_t device_id = 0;
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .address = 0,
        .address_bytes = 2,
        .miso_len = 1,
        .miso_data = &device_id,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    ESP_RETURN_ON_ERROR(spi_nand_execute_transaction(dev, &t), TAG, "%s, Failed to get the device ID %d", __func__, ret);

    dev->chip.has_quad_enable_bit = 1;
    dev->chip.quad_enable_bit_pos = 0;
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);
    switch (device_id) {
    case MACRONIX_DI_26: // MX35LF2GE4AD (2Gb)
        dev->chip.num_blocks = 2048;
        dev->chip.log2_page_size = 11;  // 2048 bytes
        dev->chip.log2_ppb = 6;         // 64 pages per block
        dev->chip.read_page_delay_us = 70;
        dev->chip.erase_block_delay_us = 6000;
        dev->chip.program_page_delay_us = 760;
        break;
    case MACRONIX_DI_37: // MX35LF4GE4AD (4Gb)
        dev->chip.num_blocks = 2048;
        dev->chip.log2_page_size = 12;  // 4096 bytes
        dev->chip.log2_ppb = 6;         // 64 pages per block
        dev->chip.read_page_delay_us = 110;
        dev->chip.erase_block_delay_us = 6000;
        dev->chip.program_page_delay_us = 800;
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
