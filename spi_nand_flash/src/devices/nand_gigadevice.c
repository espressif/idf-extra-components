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
#include "nand_gigadevice_ecc_decode.h"

static const char *TAG = "nand_gigadevice";

/* GigaDevice ECCSE lives in feature register F0h bits [5:4]; only meaningful when C0h ECCS is 01b.
 * F0h is not a standard SPI NAND feature address, so keep it vendor-local. */
#define GD_REG_STATUS_EXT  0xF0

/* ECC status decoders installed as dev->ecc_status_decoder. Only the F0h register read lives
 * here, because it needs the SPI layer, which is not built for the linux target. The bit
 * decoding is in nand_gigadevice_ecc_decode.c, which builds everywhere so host tests can
 * exercise it. */
typedef nand_ecc_status_t (*gd_ecc_pure_decode_fn)(uint8_t raw_status_c0, uint8_t raw_status_f0);

static esp_err_t gd_ecc_decode(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out,
                               gd_ecc_pure_decode_fn decode)
{
    uint8_t ext = 0;

    if (nand_gd_ecc_needs_status_ext(status_c0)) {
        ESP_RETURN_ON_ERROR(spi_nand_read_register(dev, GD_REG_STATUS_EXT, &ext), TAG,
                            "failed to read ECC status extension register");
    }
    *out = decode(status_c0, ext);
    return ESP_OK;
}

static esp_err_t gd_ecc_decode_8bit_strength(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    return gd_ecc_decode(dev, status_c0, out, nand_gd_ecc_decode_8bit_strength);
}

static esp_err_t gd_ecc_decode_4bit_strength(spi_nand_flash_device_t *dev, uint8_t status_c0, nand_ecc_status_t *out)
{
    return gd_ecc_decode(dev, status_c0, out, nand_gd_ecc_decode_4bit_strength);
}

esp_err_t spi_nand_gigadevice_init(spi_nand_flash_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    uint8_t device_id = 0;
    ESP_RETURN_ON_ERROR(spi_nand_read_device_id(dev, &device_id, sizeof(device_id)), TAG, "%s, Failed to get the device ID %d", __func__, ret);
    dev->device_info.device_id = device_id;
    snprintf(dev->device_info.chip_name, sizeof(dev->device_info.chip_name),
             "gigadevice-0x%02" PRIx8, device_id);
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);

    dev->chip.has_quad_enable_bit = 1;
    dev->chip.quad_enable_bit_pos = 0;
    dev->chip.read_page_delay_us = 25;
    dev->chip.erase_block_delay_us = 3200;
    dev->chip.program_page_delay_us = 380;
    switch (device_id) {
    case GIGADEVICE_DI_51: // GD5F1GQ5UExxG - 4 bits/528B ECC strength
    case GIGADEVICE_DI_41: // GD5F1GQ5RExxG - 4 bits/528B
    case GIGADEVICE_DI_31: // GD5F1GQ5UExxH - 4 bits/528B
    case GIGADEVICE_DI_21: // GD5F1GQ5RExxH - 4 bits/528B
        // single-plane, Internal Data Move has no parity restriction
        dev->chip.num_blocks = 1024;
        dev->ecc_status_decoder = gd_ecc_decode_4bit_strength;
        break;
    case GIGADEVICE_DI_81: // GD5F1GM7RExxG - 8 bits/528B ECC strength
    case GIGADEVICE_DI_91: // GD5F1GM7UExxG - 8 bits/528B
        // single-plane, Internal Data Move has no parity restriction
        dev->chip.num_blocks = 1024;
        dev->ecc_status_decoder = gd_ecc_decode_8bit_strength;
        break;
    case GIGADEVICE_DI_32: // GD5F2GQ5UExxH - 4 bits/528B ECC strength
    case GIGADEVICE_DI_22: // GD5F2GQ5xExxH - 4 bits/528B
        dev->chip.num_blocks = 2048;
        dev->ecc_status_decoder = gd_ecc_decode_4bit_strength;
        break;
    case GIGADEVICE_DI_52: // GD5F2GQ5UExxG - 4 bits/528B ECC strength
    case GIGADEVICE_DI_42: // GD5F2GQ5RExxG - 4 bits/528B
        // single-plane; IDM requires same odd/even block parity
        dev->chip.num_blocks = 2048;
        dev->chip.flags = NAND_FLAG_IDM_SAME_PARITY_REQUIRED;
        dev->ecc_status_decoder = gd_ecc_decode_4bit_strength;
        break;
    case GIGADEVICE_DI_92: // GD5F2GM7UExxG - 8 bits/528B ECC strength
    case GIGADEVICE_DI_82: // GD5F2GM7RExxG - 8 bits/528B
        // single-plane; IDM requires same odd/even block parity
        dev->chip.num_blocks = 2048;
        dev->chip.flags = NAND_FLAG_IDM_SAME_PARITY_REQUIRED;
        dev->ecc_status_decoder = gd_ecc_decode_8bit_strength;
        break;
    case GIGADEVICE_DI_35: // TODO: unidentified part number
    case GIGADEVICE_DI_25: // TODO: unidentified part number
        // Missing from public datasheet search. Do NOT enable the ECCSE decoder for these IDs
        // until the actual model is confirmed; dev->ecc_status_decoder is left
        // at its nand_impl.c default (2-bit).
        dev->chip.num_blocks = 4096;
        break;
    case GIGADEVICE_DI_55: // GD5F4GQ6UExxG - 4 bits/528B ECC strength
    case GIGADEVICE_DI_45: // GD5F4GQ6RExxG - 4 bits/528B
        // single-plane; IDM requires same odd/even block parity
        // (2Gb partition limit for IDM is deferred)
        dev->chip.num_blocks = 4096;
        dev->chip.flags = NAND_FLAG_IDM_SAME_PARITY_REQUIRED;
        dev->ecc_status_decoder = gd_ecc_decode_4bit_strength;
        break;
    case GIGADEVICE_DI_95: // GD5F4GM8UExxG - 8 bits/528B ECC strength
    case GIGADEVICE_DI_85: // GD5F4GM8RExxG - 8 bits/528B
        // single-plane; IDM requires same odd/even block parity
        // (2Gb partition limit for IDM is deferred)
        dev->chip.num_blocks = 4096;
        dev->chip.flags = NAND_FLAG_IDM_SAME_PARITY_REQUIRED;
        dev->ecc_status_decoder = gd_ecc_decode_8bit_strength;
        break;
    case GIGADEVICE_DI_94: // GD5F4GM7UExxG - 8 bits/528B ECC strength
        // single-plane; IDM requires same odd/even block parity
        dev->chip.log2_page_size = 12;  // 4096 bytes per page
        dev->chip.num_blocks = 2048;
        dev->chip.flags = NAND_FLAG_IDM_SAME_PARITY_REQUIRED;
        dev->ecc_status_decoder = gd_ecc_decode_8bit_strength;
        break;
    case GIGADEVICE_DI_99: // GD5F8GM8UExxG - 8 bits/528B ECC strength
        // single-plane; IDM requires same odd/even block parity
        dev->chip.log2_page_size = 12; // 4096 bytes per page
        dev->chip.num_blocks = 4096;
        dev->chip.flags = NAND_FLAG_IDM_SAME_PARITY_REQUIRED;
        dev->ecc_status_decoder = gd_ecc_decode_8bit_strength;
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}
