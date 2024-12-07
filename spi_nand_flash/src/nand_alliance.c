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

static const char *TAG = "nand_alliance";

esp_err_t spi_nand_alliance_init(spi_nand_flash_device_t *dev)
{
    uint8_t device_id;
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .address = 1,
        .address_bytes = 1,
        .dummy_bits = 8,
        .miso_len = 1,
        .miso_data = &device_id,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    spi_nand_execute_transaction(dev->config.device_handle, &t);
    dev->chip.erase_block_delay_us = 3000;
    dev->chip.program_page_delay_us = 630;
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);
    switch (device_id) {
    case ALLIANCE_DI_25: //AS5F31G04SND-08LIN
        dev->chip.num_blocks = 1024;
        dev->chip.read_page_delay_us = 60;
        break;
    case ALLIANCE_DI_2E: //AS5F32G04SND-08LIN
    case ALLIANCE_DI_8E: //AS5F12G04SND-10LIN
        dev->chip.num_blocks = 2048;
        dev->chip.read_page_delay_us = 60;
        break;
    case ALLIANCE_DI_2F: //AS5F34G04SND-08LIN
    case ALLIANCE_DI_8F: //AS5F14G04SND-10LIN
        dev->chip.num_blocks = 4096;
        dev->chip.read_page_delay_us = 60;
        break;
    case ALLIANCE_DI_2D: //AS5F38G04SND-08LIN
    case ALLIANCE_DI_8D: //AS5F18G04SND-10LIN
        dev->chip.log2_page_size = 12; // 4k pages
        dev->chip.num_blocks = 4096;
        dev->chip.read_page_delay_us = 130; // somewhat slower reads
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
