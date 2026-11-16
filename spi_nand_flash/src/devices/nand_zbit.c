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

/* ECCS (C0h [5:4]): 00 OK, 01 1-4 corrected, 10 not corrected, 11 5-8 corrected */
static const nand_ecc_status_t s_zbit_ecc_map[4] = {
    [0] = NAND_ECC_OK,
    [1] = NAND_ECC_1_TO_4_BITS_CORRECTED,
    [2] = NAND_ECC_NOT_CORRECTED,
    [3] = NAND_ECC_5_TO_8_BITS_CORRECTED,
};

static nand_ecc_status_t zbit_ecc_decode(uint8_t status_c0)
{
    return s_zbit_ecc_map[NAND_ECC_2BIT_FIELD(status_c0)];
}

static const nand_ecc_decoder_t s_zbit_ecc_decoder = { .decode = zbit_ecc_decode };

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
    dev->chip.flags = NAND_FLAG_BBM_FIRST_BYTE_ONLY;

    // Table 7-3 specifies tRD with internal ECC disabled. Poll OIP until completion.
    dev->chip.read_page_delay_us = 250;
    dev->chip.program_page_delay_us = 400;
    dev->chip.erase_block_delay_us = 3000;
    dev->ecc_decoder = &s_zbit_ecc_decoder;
    // Refresh only when ECCS reports 5-8 corrected bits.
    dev->chip.ecc_data.ecc_data_refresh_threshold = 5;
    return ESP_OK;
}
