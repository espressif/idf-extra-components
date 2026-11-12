/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "esp_check.h"
#include "nand.h"
#include "spi_nand_oper.h"
#include "nand_flash_devices.h"
#include "nand_ecc_decode.h"

static const char *TAG = "nand_zbit";

esp_err_t spi_nand_zbit_init(spi_nand_flash_device_t *dev)
{
    uint8_t device_id = 0;
    ESP_RETURN_ON_ERROR(spi_nand_read_device_id(dev, &device_id, sizeof(device_id)), TAG,
                        "Failed to get the device ID");
    if (device_id != ZBIT_DI_C1) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    dev->device_info.device_id = device_id;
    snprintf(dev->device_info.chip_name, sizeof(dev->device_info.chip_name), "ZB35Q01CYIG");
    dev->chip.log2_page_size = 11; // 2048 bytes per page
    dev->chip.log2_ppb = 6;        // 64 pages per block
    dev->chip.num_blocks = 1024;
    dev->chip.has_quad_enable_bit = 1;
    dev->chip.quad_enable_bit_pos = 0;
    dev->qio_dummy_cycles = 2;    // Datasheet V2.0, Figure 5-13 (EBh)
    dev->chip.flags = NAND_FLAG_BBM_RAW_FIRST_BYTE;

    // Initial delays from V2.0 Table 7-3; OIP is still polled until completion.
    dev->chip.read_page_delay_us = 250;
    dev->chip.program_page_delay_us = 400;
    dev->chip.erase_block_delay_us = 3000;
    dev->ecc_status_decoder = nand_ecc_decode_zbit;
    return ESP_OK;
}
