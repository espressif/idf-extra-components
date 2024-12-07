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

static const char *TAG = "nand_gigadevice";

esp_err_t spi_nand_gigadevice_init(spi_nand_flash_device_t *dev)
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
    dev->chip.read_page_delay_us = 25;
    dev->chip.erase_block_delay_us = 3200;
    dev->chip.program_page_delay_us = 380;
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);
    switch (device_id) {
    case GIGADEVICE_DI_51:
    case GIGADEVICE_DI_41:
    case GIGADEVICE_DI_31:
    case GIGADEVICE_DI_21:
        dev->chip.num_blocks = 1024;
        break;
    case GIGADEVICE_DI_52:
    case GIGADEVICE_DI_42:
    case GIGADEVICE_DI_32:
    case GIGADEVICE_DI_22:
        dev->chip.num_blocks = 2048;
        break;
    case GIGADEVICE_DI_55:
    case GIGADEVICE_DI_45:
    case GIGADEVICE_DI_35:
    case GIGADEVICE_DI_25:
        dev->chip.num_blocks = 4096;
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
