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
#include "nand_winbond_ecc_decode.h"

static const char *TAG = "nand_winbond";

/* KV-series feature register 30h: MBF [7:4] (max bit flips per sector), MFS [2:0].
 * Read with the normal Read Status Register command (0Fh + address), per the datasheet's
 * "Read Extended Internal ECC feature registers". Not a standard SPI NAND feature address,
 * so keep it vendor-local. */
#define WB_REG_ECC_MBF  0x30

/* Status Register-2 (B0h) BUF bit: 1 = Buffer Read mode, 0 = Continuous/Sequential Read mode.
 * The driver only supports Buffer Read: one page per read, column address honoured, and ECC
 * status for that page. BUF is volatile and its power-on default differs by part suffix
 * (W25N01GVxxxT powers up with BUF=0), which the JEDEC ID does not reveal. On the KV parts,
 * BUF=0 also disables internal ECC. */
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

/* ECC status decoders installed as dev->ecc_status_decoder. Only the 30h read lives here;
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

static esp_err_t wb_ecc_decode_1bit_strength(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    (void)dev;
    *out = nand_wb_ecc_decode_1bit_strength(status_c0);
    return ESP_OK;
}

/* 1-bit (Hamming) parts report at most 1 corrected bit, so the default threshold of 4 would
 * never trigger a refresh. After a 1-bit correction a sector has no margin left: one more flip
 * makes it uncorrectable. Refresh on any correction (matches Linux MTD's default
 * bitflip_threshold of 3/4 of the ECC strength, rounded up). */
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
        dev->ecc_status_decoder = wb_ecc_decode_1bit_strength;
        dev->chip.ecc_data.ecc_data_refresh_threshold = WB_1BIT_ECC_REFRESH_THRESHOLD;
        break;
    case WINBOND_DI_AA21: // W25N01GVxxxG/T/R (3.3 V) - 1 bit/528B ECC strength (Hamming)
    case WINBOND_DI_BA21: // W25N01GWxxxG/T (1.8 V) - 1 bit/528B ECC strength (Hamming)
    case WINBOND_DI_BC21: // W25N01JWxxxG/T (1.8 V) - 1 bit/528B ECC strength (Hamming)
        dev->chip.num_blocks = 1024;
        dev->ecc_status_decoder = wb_ecc_decode_1bit_strength;
        dev->chip.ecc_data.ecc_data_refresh_threshold = WB_1BIT_ECC_REFRESH_THRESHOLD;
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
    ESP_RETURN_ON_ERROR(wb_enable_buffer_read(dev), TAG, "failed to enable Buffer Read mode");
    return ESP_OK;
}
