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

static const char *TAG = "nand_winbond";

#define SWAP_BYTES(x)  (uint16_t)((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF))

esp_err_t spi_nand_winbond_init(spi_nand_flash_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    uint16_t device_id;
    ESP_RETURN_ON_ERROR(spi_nand_read_device_id(dev, (uint8_t *)&device_id, sizeof(device_id)), TAG, "%s, Failed to get the device ID %d", __func__, ret);
    device_id = SWAP_BYTES(device_id);
    dev->device_info.device_id = device_id;
    char *name = "winbond";
    strncpy(dev->device_info.chip_name, name, strlen(name) + 1);
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);

    dev->chip.has_quad_enable_bit = 0;
    dev->chip.quad_enable_bit_pos = 0;
    dev->chip.read_page_delay_us = 10;
    dev->chip.erase_block_delay_us = 2500;
    dev->chip.program_page_delay_us = 320;
    switch (device_id) {
    case WINBOND_DI_AA20:
    case WINBOND_DI_BA20:
        dev->chip.num_blocks = 512;
        break;
    case WINBOND_DI_AA21:
    case WINBOND_DI_BA21:
    case WINBOND_DI_BC21:
        dev->chip.num_blocks = 1024;
        break;
    case WINBOND_DI_AA22:
        dev->chip.num_blocks = 2048;
        break;
    case WINBOND_DI_AA23:
        dev->chip.num_blocks = 4096;
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
