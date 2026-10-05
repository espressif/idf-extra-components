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

static const char *TAG = "nand_winbond";

/* KV-series feature register 30h: MBF [7:4] (max bit flips per sector), MFS [2:0].
 * Read with the normal Read Status Register command (0Fh + address), per the datasheet's
 * "Read Extended Internal ECC feature registers". Not a standard SPI NAND feature address,
 * so keep it vendor-local. */
#define WB_REG_ECC_MBF  0x30
#define WB_REG30_MBF(reg)  (((reg) >> 4) & 0xFu)

/* B0h BUF bit. The driver needs Buffer Read mode (BUF=1), but some parts power up with
 * BUF=0, which on KV parts also disables internal ECC. */
#define WB_CONFIG_BUF   (1 << 3)

static esp_err_t wb_enable_buffer_read(spi_nand_flash_device_t *dev)
{
    uint8_t config;

    ESP_RETURN_ON_ERROR(spi_nand_read_register(dev, REG_CONFIG, &config), TAG, "failed to read config register");
    if (config & WB_CONFIG_BUF) {
        return ESP_OK;
    }
    ESP_LOGD(TAG, "BUF=0 at init (config 0x%02" PRIx8 "), switching to Buffer Read mode", config);
    return spi_nand_write_register(dev, REG_CONFIG, config | WB_CONFIG_BUF);
}

/* KV series: ECC-1:0 (C0h bits [5:4]) 01b/11b only differ by the bit flip detection
 * threshold; the exact count is read from MBF either way. */
static esp_err_t wb_kv_read_mbf(spi_nand_flash_device_t *dev, uint8_t *reg30)
{
    ESP_RETURN_ON_ERROR(spi_nand_read_register(dev, WB_REG_ECC_MBF, reg30), TAG,
                        "failed to read ECC max bit flip register");
    return ESP_OK;
}

static nand_ecc_status_t wb_kv_ecc_decode(uint8_t status_c0, uint8_t reg30)
{
    return nand_ecc_decode_eccs2_with_count(status_c0, WB_REG30_MBF(reg30));
}

static const nand_ecc_decoder_t s_wb_kv_ecc_decoder = {
    .needs_extra_read = nand_ecc_eccs2_reports_correction,
    .read_extra = wb_kv_read_mbf,
    .decode = wb_kv_ecc_decode,
};

/* 1-bit ECC parts never report more than 1 corrected bit, so refresh on any correction. */
#define WB_1BIT_ECC_REFRESH_THRESHOLD  1

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
        dev->ecc_decoder = &nand_ecc_decoder_eccs2_t1;
        dev->chip.ecc_data.ecc_data_refresh_threshold = WB_1BIT_ECC_REFRESH_THRESHOLD;
        break;
    case WINBOND_DI_AA21: // W25N01GVxxxG/T/R (3.3 V) - 1 bit/528B ECC strength (Hamming)
    case WINBOND_DI_BA21: // W25N01GWxxxG/T (1.8 V) - 1 bit/528B ECC strength (Hamming)
    case WINBOND_DI_BC21: // W25N01JWxxxG/T (1.8 V) - 1 bit/528B ECC strength (Hamming)
        dev->chip.num_blocks = 1024;
        dev->ecc_decoder = &nand_ecc_decoder_eccs2_t1;
        dev->chip.ecc_data.ecc_data_refresh_threshold = WB_1BIT_ECC_REFRESH_THRESHOLD;
        break;
    case WINBOND_DI_AA22: // W25N02KVxxIR/U (3.3 V) - 8 bits/528B ECC strength
        dev->chip.num_blocks = 2048;
        dev->ecc_decoder = &s_wb_kv_ecc_decoder;
        break;
    case WINBOND_DI_AA23: // W25N04KVxxIR/U (3.3 V) - 8 bits/528B ECC strength
        dev->chip.num_blocks = 4096;
        dev->ecc_decoder = &s_wb_kv_ecc_decoder;
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    ESP_RETURN_ON_ERROR(wb_enable_buffer_read(dev), TAG, "failed to enable Buffer Read mode");
    return ESP_OK;
}
