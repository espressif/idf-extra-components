/*
 * SPDX-FileCopyrightText: 2015-2026 Espressif Systems (Shanghai) CO LTD
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
#include "nand_ecc_decode.h"
#include "nand_winbond_ecc_decode.h"

static const char *TAG = "nand_winbond";

/* KV-series feature register 30h: MBF [7:4] (max bit flips per sector), MFS [2:0].
 * Read with the normal Read Status Register command (0Fh + address), per the datasheet's
 * "Read Extended Internal ECC feature registers". Not a standard SPI NAND feature address,
 * so keep it vendor-local. */
#define WB_REG_ECC_MBF  0x30

/* ECC status decoder installed as dev->ecc_status_decoder. Only the 30h read lives here;
 * the bit decoding is in nand_winbond_ecc_decode.c so host tests can exercise it. */
static esp_err_t wb_kv_ecc_decode(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    uint8_t mbf = 0;

    if (nand_wb_kv_ecc_needs_mbf(status_c0)) {
        ESP_RETURN_ON_ERROR(spi_nand_read_register(dev, WB_REG_ECC_MBF, &mbf), TAG,
                            "failed to read ECC max bit flip register");
    }
    *out = nand_wb_kv_ecc_decode(status_c0, mbf);
    return ESP_OK;
}

#define SWAP_BYTES(x)  (uint16_t)((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF))

esp_err_t spi_nand_winbond_init(spi_nand_flash_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    uint16_t device_id;
    ESP_RETURN_ON_ERROR(spi_nand_read_device_id(dev, (uint8_t *)&device_id, sizeof(device_id)), TAG, "%s, Failed to get the device ID %d", __func__, ret);
    device_id = SWAP_BYTES(device_id);
    dev->device_info.device_id = device_id;
    snprintf(dev->device_info.chip_name, sizeof(dev->device_info.chip_name),
             "winbond-0x%04" PRIx16, device_id);
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);

    dev->chip.has_quad_enable_bit = 0;
    dev->chip.quad_enable_bit_pos = 0;
    dev->chip.read_page_delay_us = 10;
    dev->chip.erase_block_delay_us = 2500;
    dev->chip.program_page_delay_us = 320;
    switch (device_id) {
    case WINBOND_DI_AA20: // W25N512GVxxG/T/R (3.3 V) - 1 bit/528B ECC strength (Hamming)
    case WINBOND_DI_BA20: // W25N512GWxxR/T (1.8 V) - 1 bit/528B ECC strength (Hamming)
        dev->chip.num_blocks = 512;
        dev->ecc_status_decoder = nand_ecc_decode_2bit_1bit_strength;
        break;
    case WINBOND_DI_AA21: // W25N01GVxxxG/T/R (3.3 V) - 1 bit/528B ECC strength (Hamming)
    case WINBOND_DI_BA21: // W25N01GWxxxG/T (1.8 V) - 1 bit/528B ECC strength (Hamming)
    case WINBOND_DI_BC21: // W25N01JWxxxG/T (1.8 V) - 1 bit/528B ECC strength (Hamming)
        dev->chip.num_blocks = 1024;
        dev->ecc_status_decoder = nand_ecc_decode_2bit_1bit_strength;
        break;
    case WINBOND_DI_AA22: // W25N02KVxxIR/U (3.3 V) - 8 bits/528B ECC strength
        dev->chip.num_blocks = 2048;
        dev->ecc_status_decoder = wb_kv_ecc_decode;
        break;
    case WINBOND_DI_AA23: // W25N04KVxxIR/U (3.3 V) - 8 bits/528B ECC strength
        dev->chip.num_blocks = 4096;
        dev->ecc_status_decoder = wb_kv_ecc_decode;
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
