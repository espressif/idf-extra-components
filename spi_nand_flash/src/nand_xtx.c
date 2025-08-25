/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_check.h"
#include "nand.h"
#include "spi_nand_oper.h"
#include "nand_flash_devices.h"

static const char *TAG = "nand_xtx";

esp_err_t spi_nand_xtx_init(spi_nand_flash_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    uint8_t device_id = 0;
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .address = 1,
        .address_bytes = 2,
        .miso_len = 1,
        .miso_data = &device_id,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    ESP_RETURN_ON_ERROR(spi_nand_execute_transaction(dev, &t), TAG, "%s, Failed to get the device ID %d", __func__, ret);

    dev->chip.has_quad_enable_bit = 1;
    dev->chip.quad_enable_bit_pos = 0;
    dev->chip.erase_block_delay_us = 3500;
    dev->chip.program_page_delay_us = 650;
    dev->chip.read_page_delay_us = 50;
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);
    switch (device_id) {
    case XTX_DI_37: //XT26G08D
        dev->chip.num_blocks = 4096;
        dev->chip.log2_ppb = 6;        // 64 pages per block
        dev->chip.log2_page_size = 12; // 4096 bytes per page
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
