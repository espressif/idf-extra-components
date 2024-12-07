/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_check.h"
#include "nand.h"
#include "spi_nand_oper.h"
#include "nand_flash_devices.h"

static const char *TAG = "nand_micron";

esp_err_t spi_nand_micron_init(spi_nand_flash_device_t *dev)
{
    uint8_t device_id;
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .dummy_bits = 16,
        .miso_len = 1,
        .miso_data = &device_id,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    spi_nand_execute_transaction(dev->config.device_handle, &t);
    dev->chip.ecc_data.ecc_status_reg_len_in_bits = 3;
    dev->chip.erase_block_delay_us = 2000;
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);
    switch (device_id) {
    case MICRON_DI_34:
        dev->chip.read_page_delay_us = 115;
        dev->chip.program_page_delay_us = 240;
        dev->chip.num_blocks = 2048;
        dev->chip.log2_ppb = 6;        // 64 pages per block
        dev->chip.log2_page_size = 12; // 4096 bytes per page
        break;
    case MICRON_DI_14:
    case MICRON_DI_15:
        dev->chip.read_page_delay_us = 46;
        dev->chip.program_page_delay_us = 220;
        dev->chip.num_blocks = 1024;
        dev->chip.log2_ppb = 6;          // 64 pages per block
        dev->chip.log2_page_size = 11;   // 2048 bytes per page
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
