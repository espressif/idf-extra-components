/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2024 Espressif Systems (Shanghai) CO LTD
 */

#include <string.h>
#include "esp_check.h"
#include "esp_err.h"
#include "spi_nand_oper.h"
#include "spi_nand_flash.h"
#include "nand.h"

#define ROM_WAIT_THRESHOLD_US 1000

static const char *TAG = "spi_nand";

#if CONFIG_NAND_FLASH_VERIFY_WRITE
static esp_err_t s_verify_write(spi_nand_flash_device_t *handle, const uint8_t *expected_buffer, uint16_t offset, uint16_t length)
{
    uint8_t *temp_buf = NULL;
    temp_buf = heap_caps_malloc(length, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(temp_buf != NULL, ESP_ERR_NO_MEM, TAG, "nomem");
    if (spi_nand_read(handle, temp_buf, offset, length)) {
        ESP_LOGE(TAG, "%s: Failed to read nand flash to verify previous write", __func__);
        free(temp_buf);
        return ESP_FAIL;
    }

    if (memcmp(temp_buf, expected_buffer, length)) {
        ESP_LOGE(TAG, "%s: Data mismatch detected. The previously written buffer does not match the read buffer.", __func__);
        free(temp_buf);
        return ESP_FAIL;
    }
    free(temp_buf);
    return ESP_OK;
}
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE

static esp_err_t wait_for_ready(spi_nand_flash_device_t *dev, uint32_t expected_operation_time_us, uint8_t *status_out)
{
    if (expected_operation_time_us < ROM_WAIT_THRESHOLD_US) {
        esp_rom_delay_us(expected_operation_time_us);
    }

    while (true) {
        uint8_t status;
        ESP_RETURN_ON_ERROR(spi_nand_read_register(dev, REG_STATUS, &status), TAG, "");

        if ((status & STAT_BUSY) == 0) {
            if (status_out) {
                *status_out = status;
            }
            break;
        }

        if (expected_operation_time_us >= ROM_WAIT_THRESHOLD_US) {
            vTaskDelay(1);
        }
    }

    return ESP_OK;
}

static esp_err_t read_page_and_wait(spi_nand_flash_device_t *dev, uint32_t page, uint8_t *status_out)
{
    ESP_RETURN_ON_ERROR(spi_nand_read_page(dev, page), TAG, "");

    return wait_for_ready(dev, dev->chip.read_page_delay_us, status_out);
}

static esp_err_t program_execute_and_wait(spi_nand_flash_device_t *dev, uint32_t page, uint8_t *status_out)
{
    ESP_RETURN_ON_ERROR(spi_nand_program_execute(dev, page), TAG, "");

    return wait_for_ready(dev, dev->chip.program_page_delay_us, status_out);
}

static uint16_t get_column_address(spi_nand_flash_device_t *handle, uint32_t block, uint32_t offset)
{
    uint16_t column_addr = offset;

    if ((handle->chip.flags & NAND_FLAG_HAS_READ_PLANE_SELECT) || (handle->chip.flags & NAND_FLAG_HAS_PROG_PLANE_SELECT)) {
        uint32_t plane = block % handle->chip.num_planes;
        // The plane index is the bit following the most significant bit (MSB) of the address.
        // For a 2048-byte page (2^11), the plane select bit is the 12th bit, and
        // for a 4096-byte page (2^12), it is the 13th bit.
        column_addr += plane << (handle->chip.log2_page_size + 1);
    }
    return column_addr;
}

esp_err_t nand_is_bad(spi_nand_flash_device_t *handle, uint32_t block, bool *is_bad_status)
{
    uint32_t first_block_page = block * (1 << handle->chip.log2_ppb);
    uint16_t bad_block_indicator;
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, first_block_page, NULL), fail, TAG, "");

    uint16_t column_addr = get_column_address(handle, block, handle->chip.page_size);

    // Read the first 2 bytes on the OOB of the first page in the block. This should be 0xFFFF for a good block
    ESP_GOTO_ON_ERROR(spi_nand_read(handle, (uint8_t *) handle->read_buffer, column_addr, 2),
                      fail, TAG, "");

    memcpy(&bad_block_indicator, handle->read_buffer, sizeof(bad_block_indicator));
    ESP_LOGD(TAG, "is_bad, block=%"PRIu32", page=%"PRIu32",indicator = %04x", block, first_block_page, bad_block_indicator);
    *is_bad_status = (bad_block_indicator != 0xFFFF);
    return ret;

fail:
    ESP_LOGE(TAG, "Error in nand_is_bad %d", ret);
    return ret;
}

esp_err_t nand_mark_bad(spi_nand_flash_device_t *handle, uint32_t block)
{
    esp_err_t ret = ESP_OK;

    uint32_t first_block_page = block * (1 << handle->chip.log2_ppb);
    uint16_t bad_block_indicator = 0;
    uint8_t status;
    ESP_LOGD(TAG, "mark_bad, block=%"PRIu32", page=%"PRIu32",indicator = %04x", block, first_block_page, bad_block_indicator);

    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, first_block_page, NULL), fail, TAG, "");
    ESP_GOTO_ON_ERROR(spi_nand_write_enable(handle), fail, TAG, "");
    ESP_GOTO_ON_ERROR(spi_nand_erase_block(handle, first_block_page),
                      fail, TAG, "");
    ESP_GOTO_ON_ERROR(wait_for_ready(handle, handle->chip.erase_block_delay_us, &status),
                      fail, TAG, "");
    if ((status & STAT_ERASE_FAILED) != 0) {
        ret = ESP_ERR_NOT_FINISHED;
        goto fail;
    }

    ESP_GOTO_ON_ERROR(spi_nand_write_enable(handle), fail, TAG, "");

    uint16_t column_addr = get_column_address(handle, block, handle->chip.page_size);

    ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, (const uint8_t *) &bad_block_indicator,
                                            column_addr, 2), fail, TAG, "");
    ESP_GOTO_ON_ERROR(program_execute_and_wait(handle, first_block_page, NULL), fail, TAG, "");

#if CONFIG_NAND_FLASH_VERIFY_WRITE
    ret = s_verify_write(handle, (uint8_t *)&bad_block_indicator, column_addr, 2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s: mark_bad write verification failed for block=%"PRIu32" and page=%"PRIu32"", __func__, block, first_block_page);
    }
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE
    return ret;
fail:
    ESP_LOGE(TAG, "Error in nand_mark_bad %d", ret);
    return ret;
}

esp_err_t nand_erase_chip(spi_nand_flash_device_t *handle)
{
    esp_err_t ret = ESP_OK;
    uint8_t status;

    for (int i = 0; i < handle->chip.num_blocks; i++) {
        ESP_GOTO_ON_ERROR(spi_nand_write_enable(handle), end, TAG, "");
        ESP_GOTO_ON_ERROR(spi_nand_erase_block(handle, i * (1 << handle->chip.log2_ppb)),
                          end, TAG, "");
        ESP_GOTO_ON_ERROR(wait_for_ready(handle, handle->chip.erase_block_delay_us, &status),
                          end, TAG, "");
        if ((status & STAT_ERASE_FAILED) != 0) {
            ret = ESP_ERR_NOT_FINISHED;
        }
    }
    return ret;

end:
    ESP_LOGE(TAG, "Error in nand_erase_chip %d", ret);
    return ret;
}

esp_err_t nand_erase_block(spi_nand_flash_device_t *handle, uint32_t block)
{
    ESP_LOGD(TAG, "erase_block, block=%"PRIu32",", block);
    esp_err_t ret = ESP_OK;
    uint8_t status;

    uint32_t first_block_page = block * (1 << handle->chip.log2_ppb);

    ESP_GOTO_ON_ERROR(spi_nand_write_enable(handle), fail, TAG, "");
    ESP_GOTO_ON_ERROR(spi_nand_erase_block(handle, first_block_page),
                      fail, TAG, "");
    ESP_GOTO_ON_ERROR(wait_for_ready(handle,
                                     handle->chip.erase_block_delay_us, &status),
                      fail, TAG, "");

    if ((status & STAT_ERASE_FAILED) != 0) {
        ret = ESP_ERR_NOT_FINISHED;
    }
    return ret;

fail:
    ESP_LOGE(TAG, "Error in nand_erase %d", ret);
    return ret;
}

esp_err_t nand_prog(spi_nand_flash_device_t *handle, uint32_t page, const uint8_t *data)
{
    ESP_LOGV(TAG, "prog, page=%"PRIu32",", page);
    esp_err_t ret = ESP_OK;
    uint16_t used_marker = 0;
    uint8_t status;

    uint32_t block = page >> handle->chip.log2_ppb;
    uint16_t column_addr = get_column_address(handle, block, 0);

    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, page, NULL), fail, TAG, "");
    ESP_GOTO_ON_ERROR(spi_nand_write_enable(handle), fail, TAG, "");
    ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, data, column_addr, handle->chip.page_size),
                      fail, TAG, "");
    ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, (uint8_t *)&used_marker,
                                            column_addr + handle->chip.page_size + 2, 2), fail, TAG, "");

    ESP_GOTO_ON_ERROR(program_execute_and_wait(handle, page, &status), fail, TAG, "");

    if ((status & STAT_PROGRAM_FAILED) != 0) {
        ESP_LOGD(TAG, "prog failed, page=%"PRIu32",", page);
        return ESP_ERR_NOT_FINISHED;
    }

#if CONFIG_NAND_FLASH_VERIFY_WRITE
    ret = s_verify_write(handle, data, column_addr, handle->chip.page_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s: prog page=%"PRIu32" write verification failed", __func__, page);
    }
    ret = s_verify_write(handle, (uint8_t *)&used_marker, column_addr + handle->chip.page_size + 2, 2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s: prog page=%"PRIu32" used marker write verification failed", __func__, page);
    }
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE

    return ret;
fail:
    ESP_LOGE(TAG, "Error in nand_prog %d", ret);
    return ret;
}

esp_err_t nand_is_free(spi_nand_flash_device_t *handle, uint32_t page, bool *is_free_status)
{
    esp_err_t ret = ESP_OK;
    uint16_t used_marker;

    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, page, NULL), fail, TAG, "");

    uint32_t block = page >> handle->chip.log2_ppb;
    uint16_t column_addr = get_column_address(handle, block, handle->chip.page_size + 2);

    ESP_GOTO_ON_ERROR(spi_nand_read(handle, (uint8_t *)handle->read_buffer,
                                    column_addr, 2), fail, TAG, "");

    memcpy(&used_marker, handle->read_buffer, sizeof(used_marker));
    ESP_LOGD(TAG, "is free, page=%"PRIu32", used_marker=%04x,", page, used_marker);
    *is_free_status = (used_marker == 0xFFFF);
    return ret;
fail:
    ESP_LOGE(TAG, "Error in nand_is_free %d", ret);
    return ret;
}

#define PACK_2BITS_STATUS(status, bit1, bit0)         ((((status) & (bit1)) << 1) | ((status) & (bit0)))
#define PACK_3BITS_STATUS(status, bit2, bit1, bit0)   ((((status) & (bit2)) << 2) | (((status) & (bit1)) << 1) | ((status) & (bit0)))

static bool is_ecc_error(spi_nand_flash_device_t *dev, uint8_t status)
{
    bool is_ecc_err = false;
    ecc_status_t bits_corrected_status = STAT_ECC_OK;
    if (dev->chip.ecc_data.ecc_status_reg_len_in_bits == 2) {
        bits_corrected_status = PACK_2BITS_STATUS(status, STAT_ECC1, STAT_ECC0);
    } else if (dev->chip.ecc_data.ecc_status_reg_len_in_bits == 3) {
        bits_corrected_status = PACK_3BITS_STATUS(status, STAT_ECC2, STAT_ECC1, STAT_ECC0);
    } else {
        bits_corrected_status = STAT_ECC_MAX;
    }
    dev->chip.ecc_data.ecc_corrected_bits_status = bits_corrected_status;
    if (bits_corrected_status) {
        if (bits_corrected_status == STAT_ECC_MAX) {
            ESP_LOGE(TAG, "%s: Error while initializing value of ecc_status_reg_len_in_bits", __func__);
            is_ecc_err = true;
        } else if (bits_corrected_status == STAT_ECC_NOT_CORRECTED) {
            is_ecc_err = true;
        }
    }
    return is_ecc_err;
}

esp_err_t nand_read(spi_nand_flash_device_t *handle, uint32_t page, size_t offset, size_t length, uint8_t *data)
{
    ESP_LOGV(TAG, "read, page=%"PRIu32", offset=%d, length=%d", page, offset, length);
    assert(page < handle->chip.num_blocks * (1 << handle->chip.log2_ppb));
    esp_err_t ret = ESP_OK;
    uint8_t status;

    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, page, &status), fail, TAG, "");

    if (is_ecc_error(handle, status)) {
        ESP_LOGD(TAG, "read ecc error, page=%"PRIu32"", page);
        return ESP_FAIL;
    }

    uint32_t block = page >> handle->chip.log2_ppb;
    uint16_t column_addr = get_column_address(handle, block, offset);

    ESP_GOTO_ON_ERROR(spi_nand_read(handle, data, column_addr, length), fail, TAG, "");

    return ret;
fail:
    ESP_LOGE(TAG, "Error in nand_read %d", ret);
    return ret;
}

esp_err_t nand_copy(spi_nand_flash_device_t *handle, uint32_t src, uint32_t dst)
{
    ESP_LOGD(TAG, "copy, src=%"PRIu32", dst=%"PRIu32"", src, dst);
    esp_err_t ret = ESP_OK;
#if CONFIG_NAND_FLASH_VERIFY_WRITE
    uint8_t *temp_buf = NULL;
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE

    uint8_t status;
    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, src, &status), fail, TAG, "");

    if (is_ecc_error(handle, status)) {
        ESP_LOGD(TAG, "copy, ecc error");
        return ESP_FAIL;
    }

    ESP_GOTO_ON_ERROR(spi_nand_write_enable(handle), fail, TAG, "");
    uint32_t src_block = src >> handle->chip.log2_ppb;
    uint32_t dst_block = dst >> handle->chip.log2_ppb;
    uint16_t src_column_addr = get_column_address(handle, src_block, 0);
    uint16_t dst_column_addr = get_column_address(handle, dst_block, 0);

    if (src_column_addr != dst_column_addr) {
        // In a 2 plane structure of the flash, if the pages are not on the same plane, the data must be copied through RAM.
        uint8_t *copy_buf = heap_caps_malloc(handle->chip.page_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        ESP_GOTO_ON_FALSE(copy_buf, ESP_ERR_NO_MEM, fail, TAG, "Failed to allocate copy buffer");

        ESP_GOTO_ON_ERROR(spi_nand_read(handle, copy_buf, src_column_addr, handle->chip.page_size), fail, TAG, "");

        ESP_GOTO_ON_ERROR(spi_nand_write_enable(handle), fail, TAG, "");

        ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, copy_buf, dst_column_addr, handle->chip.page_size),
                          fail, TAG, "");

        uint16_t used_marker = 0;
        ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, (uint8_t *)&used_marker,
                                                dst_column_addr + handle->chip.page_size + 2, 2), fail, TAG, "");
        ESP_GOTO_ON_ERROR(program_execute_and_wait(handle, dst, &status), fail, TAG, "");

        if ((status & STAT_PROGRAM_FAILED) != 0) {
            ESP_LOGD(TAG, "copy, prog failed");
            return ESP_ERR_NOT_FINISHED;
        }
        free(copy_buf);
    }

    ESP_GOTO_ON_ERROR(program_execute_and_wait(handle, dst, &status), fail, TAG, "");
    if ((status & STAT_PROGRAM_FAILED) != 0) {
        ESP_LOGD(TAG, "copy, prog failed");
        return ESP_ERR_NOT_FINISHED;
    }

#if CONFIG_NAND_FLASH_VERIFY_WRITE
    // First read src page data from cache to temp_buf
    if (src_column_addr != dst_column_addr) {
        // Then read src page data from nand memory array and load it in cache
        ESP_GOTO_ON_ERROR(read_page_and_wait(handle, src, &status), fail, TAG, "");
        if (is_ecc_error(handle, status)) {
            ESP_LOGE(TAG, "%s: dst_page=%"PRIu32" read, ecc error", __func__, dst);
            goto fail;
        }
    }

    temp_buf = heap_caps_malloc(handle->chip.page_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(temp_buf != NULL, ESP_ERR_NO_MEM, TAG, "nomem");
    if (spi_nand_read(handle, temp_buf, src_column_addr, handle->chip.page_size)) {
        ESP_LOGE(TAG, "%s: Failed to read src_page=%"PRIu32"", __func__, src);
        goto fail;
    }
    // Then read dst page data from nand memory array and load it in cache
    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, dst, &status), fail, TAG, "");
    if (is_ecc_error(handle, status)) {
        ESP_LOGE(TAG, "%s: dst_page=%"PRIu32" read, ecc error", __func__, dst);
        goto fail;
    }
    // Check if the data in the src page matches the dst page
    ret = s_verify_write(handle, temp_buf, dst_column_addr, handle->chip.page_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s: dst_page=%"PRIu32" write verification failed", __func__, dst);
    }

    free(temp_buf);
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE
    return ret;

fail:
#if CONFIG_NAND_FLASH_VERIFY_WRITE
    free(temp_buf);
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE
    ESP_LOGE(TAG, "Error in nand_copy %d", ret);
    return ret;
}

esp_err_t nand_get_ecc_status(spi_nand_flash_device_t *handle, uint32_t page)
{
    esp_err_t ret = ESP_OK;
    uint8_t status;
    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, page, &status), fail, TAG, "");

    if (is_ecc_error(handle, status)) {
        ESP_LOGD(TAG, "read ecc error, page=%"PRIu32"", page);
    }
    return ret;

fail:
    ESP_LOGE(TAG, "Error in nand_is_ecc_error %d", ret);
    return ret;
}
