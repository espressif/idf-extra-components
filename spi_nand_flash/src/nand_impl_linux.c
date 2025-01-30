/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_check.h"
#include "esp_err.h"
#include "spi_nand_flash.h"
#include "nand.h"
#include "nand_linux_mmap_emul.h"

static const char *TAG = "nand_linux";

esp_err_t nand_is_bad(spi_nand_flash_device_t *handle, uint32_t block, bool *is_bad_status)
{
    uint16_t bad_block_indicator = 0xFFFF;
    esp_err_t ret = ESP_OK;
    uint32_t block_offset = block * handle->chip.block_size;

    // Read the first 2 bytes on the OOB of the first page in the block. This should be 0xFFFF for a good block
    ESP_RETURN_ON_ERROR(nand_emul_read(handle, block_offset + handle->chip.page_size, (uint8_t *) &bad_block_indicator, 2),
                        TAG, "Error in nand_is_bad %d", ret);

    ESP_LOGD(TAG, "is_bad, block=%"PRIu32", page=%"PRIu32",indicator = %04x", block, block_offset, bad_block_indicator);
    *is_bad_status = bad_block_indicator != 0xFFFF;
    return ret;
}

esp_err_t nand_mark_bad(spi_nand_flash_device_t *handle, uint32_t block)
{
    esp_err_t ret = ESP_OK;

    uint32_t first_block_page = block * (1 << handle->chip.log2_ppb);
    uint16_t bad_block_indicator = 0;
    ESP_LOGD(TAG, "mark_bad, block=%"PRIu32", page=%"PRIu32",indicator = %04x", block, first_block_page, bad_block_indicator);

    ESP_RETURN_ON_ERROR(nand_emul_erase_block(handle, block * handle->chip.block_size), TAG, "Error in nand_mark_bad %d", ret);

    ESP_RETURN_ON_ERROR(nand_emul_write(handle, block * handle->chip.block_size + handle->chip.page_size,
                                        (const uint8_t *) &bad_block_indicator, 2), TAG, "Error in nand_mark_bad %d", ret);

    return ret;
}

esp_err_t nand_erase_block(spi_nand_flash_device_t *handle, uint32_t block)
{
    ESP_LOGD(TAG, "erase_block, block=%"PRIu32",", block);
    esp_err_t ret = ESP_OK;

    uint32_t address = block * handle->chip.block_size;

    ESP_RETURN_ON_ERROR(nand_emul_erase_block(handle, address), TAG, "Error in nand_erase %x", ret);
    return ret;
}

esp_err_t nand_erase_chip(spi_nand_flash_device_t *handle)
{
    esp_err_t ret = ESP_OK;

    for (int i = 0; i < handle->chip.num_blocks; i++) {
        ESP_RETURN_ON_ERROR(nand_erase_block(handle, i), TAG, "Error in nand_erase_chip %d", ret);
    }
    return ret;
}

esp_err_t nand_prog(spi_nand_flash_device_t *handle, uint32_t page, const uint8_t *data)
{
    ESP_LOGV(TAG, "prog, page=%"PRIu32",", page);
    esp_err_t ret = ESP_OK;
    uint16_t used_marker = 0;
    uint32_t data_offset = page * handle->chip.emulated_page_size;

    ESP_RETURN_ON_ERROR(nand_emul_write(handle, data_offset, data, handle->chip.page_size), TAG, "Error in nand_prog %d", ret);
    ESP_RETURN_ON_ERROR(nand_emul_write(handle, data_offset + handle->chip.page_size + 2,
                                        (uint8_t *)&used_marker, 2), TAG, "Error in nand_prog %d", ret);

    return ret;
}

esp_err_t nand_is_free(spi_nand_flash_device_t *handle, uint32_t page, bool *is_free_status)
{
    esp_err_t ret = ESP_OK;
    uint16_t used_marker = 0xFF;

    ESP_RETURN_ON_ERROR(nand_emul_read(handle, page * handle->chip.emulated_page_size + handle->chip.page_size + 2, (uint8_t *)&used_marker, 2),
                        TAG, "Error in nand_is_free %d", ret);

    ESP_LOGD(TAG, "is free, page=%"PRIu32", used_marker=%04x,", page, used_marker);
    *is_free_status = (used_marker == 0xFFFF);
    return ret;
}

esp_err_t nand_read(spi_nand_flash_device_t *handle, uint32_t page, size_t offset, size_t length, uint8_t *data)
{
    ESP_LOGV(TAG, "read, page=%"PRIu32", offset=%ld, length=%ld", page, offset, length);
    assert(page < handle->chip.num_blocks * (1 << handle->chip.log2_ppb));
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_ERROR(nand_emul_read(handle, page * handle->chip.emulated_page_size + offset, data, length),
                        TAG, "Error in nand_read %d", ret);

    return ret;
}

esp_err_t nand_copy(spi_nand_flash_device_t *handle, uint32_t src, uint32_t dst)
{
    ESP_LOGD(TAG, "copy, src=%"PRIu32", dst=%"PRIu32"", src, dst);
    esp_err_t ret = ESP_OK;
    uint32_t dst_offset = dst * handle->chip.emulated_page_size;
    uint32_t src_offset = src * handle->chip.emulated_page_size;
    ESP_RETURN_ON_ERROR(nand_emul_read(handle, (size_t)src_offset, (void *)handle->read_buffer, handle->chip.page_size),
                        TAG, "Error in nand_copy %d", ret);
    ESP_RETURN_ON_ERROR(nand_emul_write(handle, (size_t)dst_offset, (void *)handle->read_buffer, handle->chip.page_size),
                        TAG, "Error in nand_copy %d", ret);

    return ret;
}

esp_err_t nand_get_ecc_status(spi_nand_flash_device_t *handle, uint32_t page)
{
    esp_err_t ret = ESP_OK;
    return ret;
}
