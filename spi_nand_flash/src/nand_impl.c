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
#include "nand.h"
#include "nand_flash_devices.h"
#include "nand_device_types.h"
#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT
#include "nand_oob_device.h"
#include "nand_oob_xfer.h"
#endif

#define ROM_WAIT_THRESHOLD_US 1000

static const char *TAG = "nand_hal";

static esp_err_t detect_chip(spi_nand_flash_device_t *dev)
{
    uint8_t manufacturer_id = 0;
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_ERROR(spi_nand_read_manufacturer_id(dev, &manufacturer_id), TAG, "%s, Failed to get the manufacturer ID %d", __func__, ret);
    ESP_LOGD(TAG, "%s: manufacturer_id: %x\n", __func__, manufacturer_id);
    dev->device_info.manufacturer_id = manufacturer_id;

    switch (manufacturer_id) {
    case SPI_NAND_FLASH_ALLIANCE_MI: // Alliance
        return spi_nand_alliance_init(dev);
    case SPI_NAND_FLASH_WINBOND_MI: // Winbond
        return spi_nand_winbond_init(dev);
    case SPI_NAND_FLASH_GIGADEVICE_MI: // GigaDevice
        return spi_nand_gigadevice_init(dev);
    case SPI_NAND_FLASH_MICRON_MI: // Micron
        return spi_nand_micron_init(dev);
    case SPI_NAND_FLASH_ZETTA_MI: // Zetta
        return spi_nand_zetta_init(dev);
    case SPI_NAND_FLASH_XTX_MI: // XTX
        return spi_nand_xtx_init(dev);
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
}

static esp_err_t enable_quad_io_mode(spi_nand_flash_device_t *dev)
{
    uint8_t io_config;
    esp_err_t ret = spi_nand_read_register(dev, REG_CONFIG, &io_config);
    if (ret != ESP_OK) {
        return ret;
    }

    io_config |= (1 << dev->chip.quad_enable_bit_pos);
    ESP_LOGD(TAG, "%s: quad config register value: 0x%x", __func__, io_config);

    if (io_config != 0x00) {
        ret = spi_nand_write_register(dev, REG_CONFIG, io_config);
    }

    return ret;
}

static esp_err_t unprotect_chip(spi_nand_flash_device_t *dev)
{
    uint8_t status;
    esp_err_t ret = spi_nand_read_register(dev, REG_PROTECT, &status);
    if (ret != ESP_OK) {
        return ret;
    }

    if (status != 0x00) {
        ret = spi_nand_write_register(dev, REG_PROTECT, 0);
    }

    return ret;
}

esp_err_t nand_init_device(spi_nand_flash_config_t *config, spi_nand_flash_device_t **handle)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(config->device_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Spi device pointer can not be NULL");

    *handle = heap_caps_calloc(1, sizeof(spi_nand_flash_device_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (*handle == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memcpy(&(*handle)->config, config, sizeof(spi_nand_flash_config_t));

    (*handle)->chip.ecc_data.ecc_status_reg_len_in_bits = 2;
    (*handle)->chip.ecc_data.ecc_data_refresh_threshold = 4;
    (*handle)->chip.log2_ppb = 6;         // 64 pages per block is standard
    (*handle)->chip.log2_page_size = 11;  // 2048 bytes per page is fairly standard
    (*handle)->chip.num_planes = 1;
    (*handle)->chip.flags = 0;

    ESP_GOTO_ON_ERROR(detect_chip(*handle), fail, TAG, "Failed to detect nand chip");
    ESP_GOTO_ON_ERROR(unprotect_chip(*handle), fail, TAG, "Failed to clear protection register");

    if (((*handle)->config.io_mode ==  SPI_NAND_IO_MODE_QOUT || (*handle)->config.io_mode ==  SPI_NAND_IO_MODE_QIO)
            && (*handle)->chip.has_quad_enable_bit) {
        ESP_GOTO_ON_ERROR(enable_quad_io_mode(*handle), fail, TAG, "Failed to enable quad mode");
    }

    (*handle)->chip.page_size = 1 << (*handle)->chip.log2_page_size;
    (*handle)->chip.block_size = (1 << (*handle)->chip.log2_ppb) * (*handle)->chip.page_size;

    size_t dma_alignment = spi_nand_get_dma_alignment();
    (*handle)->work_buffer = heap_caps_aligned_alloc(dma_alignment, (*handle)->chip.page_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE((*handle)->work_buffer != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");

    (*handle)->read_buffer = heap_caps_aligned_alloc(dma_alignment, (*handle)->chip.page_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE((*handle)->read_buffer != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");

    (*handle)->temp_buffer = heap_caps_aligned_alloc(dma_alignment, (*handle)->chip.page_size + dma_alignment, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE((*handle)->temp_buffer != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");

#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT
    ESP_GOTO_ON_ERROR(nand_oob_device_layout_init(*handle), fail, TAG, "OOB layout init");
#endif

    (*handle)->mutex = xSemaphoreCreateMutex();
    if (!(*handle)->mutex) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }
    return ret;

fail:
    free((*handle)->work_buffer);
    free((*handle)->read_buffer);
    free((*handle)->temp_buffer);
    if ((*handle)->mutex) {
        vSemaphoreDelete((*handle)->mutex);
    }
    free(*handle);
    return ret;
}

/***************************************************************************************/

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
        if (handle->chip.num_planes == 0) {
            ESP_LOGE(TAG, "Invalid number of planes (0)");
            return column_addr;  // Return offset without plane selection
        }
        uint32_t plane = block % handle->chip.num_planes;
        // The plane index is the bit following the most significant bit (MSB) of the address.
        // For a 2048-byte page (2^11), the plane select bit is the 12th bit, and
        // for a 4096-byte page (2^12), it is the 13th bit.
        column_addr += plane << (handle->chip.log2_page_size + 1);
    }
    return column_addr;
}

#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT
static bool nand_oob_bbm_good(const spi_nand_oob_layout_t *layout, const uint8_t *oob_prefix)
{
    return memcmp(oob_prefix + layout->bbm.bbm_offset, layout->bbm.good_pattern, layout->bbm.bbm_length) == 0;
}

static uint32_t nand_oob_bbm_check_page(spi_nand_flash_device_t *handle, uint32_t block)
{
    uint32_t first_block_page = block * (1 << handle->chip.log2_ppb);
    uint32_t mask = handle->oob_layout->bbm.check_pages_mask;

    if (mask & SPI_NAND_BBM_CHECK_FIRST_PAGE) {
        return first_block_page;
    }
    if (mask & SPI_NAND_BBM_CHECK_LAST_PAGE) {
        return first_block_page + (1u << handle->chip.log2_ppb) - 1;
    }
    return first_block_page;
}

static uint16_t nand_impl_effective_oob_size(const spi_nand_flash_device_t *handle)
{
    if (handle->oob_layout->oob_bytes != 0) {
        return (uint16_t)handle->oob_layout->oob_bytes;
    }
    switch (handle->chip.page_size) {
    case 512:
        return 16;
    case 2048:
        return 64;
    case 4096:
        return 128;
    default:
        return 0;
    }
}
#endif

esp_err_t nand_is_bad(spi_nand_flash_device_t *handle, uint32_t block, bool *is_bad_status)
{
    esp_err_t ret = ESP_OK;

#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT
    uint32_t bbm_page = nand_oob_bbm_check_page(handle, block);
    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, bbm_page, NULL), fail, TAG, "");

    uint16_t column_addr = get_column_address(handle, block, handle->chip.page_size);

    // Read 4 bytes to include both bad block marker and page status
    ESP_GOTO_ON_ERROR(spi_nand_read(handle, (uint8_t *) handle->read_buffer, column_addr, 4),
                      fail, TAG, "");

    ESP_LOGD(TAG, "is_bad, block=%"PRIu32", page=%"PRIu32",indicator = %02x,%02x", block, bbm_page,
             handle->read_buffer[0], handle->read_buffer[1]);
    *is_bad_status = !nand_oob_bbm_good(handle->oob_layout, handle->read_buffer);
    return ret;
#else
    uint32_t first_block_page = block * (1 << handle->chip.log2_ppb);
    // Markers layout: [bad_block_marker (bytes 0-1)][page_used_marker (bytes 2-3)]
    uint8_t markers[4];
    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, first_block_page, NULL), fail, TAG, "");

    uint16_t column_addr = get_column_address(handle, block, handle->chip.page_size);

    // Read 4 bytes to include both bad block marker and page status
    ESP_GOTO_ON_ERROR(spi_nand_read(handle, (uint8_t *) handle->read_buffer, column_addr, 4),
                      fail, TAG, "");

    memcpy(&markers, handle->read_buffer, sizeof(markers));
    ESP_LOGD(TAG, "is_bad, block=%"PRIu32", page=%"PRIu32",indicator = %02x,%02x", block, first_block_page, markers[0], markers[1]);
    *is_bad_status = (markers[0] != 0xFF || markers[1] != 0xFF);
    return ret;
#endif

fail:
    ESP_LOGE(TAG, "Error in nand_is_bad %d", ret);
    return ret;
}

esp_err_t nand_mark_bad(spi_nand_flash_device_t *handle, uint32_t block)
{
    esp_err_t ret = ESP_OK;

    uint32_t first_block_page = block * (1 << handle->chip.log2_ppb);
    uint8_t status;
    ESP_LOGD(TAG, "mark_bad, block=%"PRIu32", page=%"PRIu32"", block, first_block_page);

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

#ifndef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT
    // Markers layout: [bad_block_marker (bytes 0-1)][page_used_marker (bytes 2-3)]
    const uint8_t markers_legacy[4] = { 0x00, 0x00, 0xFF, 0xFF }; //// 0x0000 (bad block), 0xFFFF (free)
#endif
#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT
    uint8_t *scratch = handle->temp_buffer;
    memset(scratch, 0xFF, 4);
    for (unsigned i = 0; i < handle->oob_layout->bbm.bbm_length; i++) {
        scratch[handle->oob_layout->bbm.bbm_offset + i] = (uint8_t)(handle->oob_layout->bbm.good_pattern[i] ^ 0xFF);
    }
    ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, scratch, column_addr, 4), fail, TAG, "");
#else
    // Write 4 bytes: bad block marker (0x0000) + page used marker (0xFFFF)
    ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, markers_legacy, column_addr, 4), fail, TAG, "");
#endif
    ESP_GOTO_ON_ERROR(program_execute_and_wait(handle, first_block_page, NULL), fail, TAG, "");

#if CONFIG_NAND_FLASH_VERIFY_WRITE
#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT
    ret = s_verify_write(handle, handle->temp_buffer, column_addr, 4);
#else
    ret = s_verify_write(handle, (uint8_t *)markers_legacy, column_addr, 4);
#endif
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s: mark_bad write verification failed for block=%"PRIu32" and page=%"PRIu32"", __func__, block, first_block_page);
    }
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE
    return ret;
fail:
    ESP_LOGE(TAG, "Error in nand_mark_bad %d", ret);
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

static esp_err_t nand_erase_good_block(spi_nand_flash_device_t *handle, uint32_t block)
{
    ESP_LOGD(TAG, "erase_block, block=%"PRIu32",", block);
    esp_err_t ret = ESP_OK;
    bool is_bad = false;
    ret = nand_is_bad(handle, block, &is_bad);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error querying bad block status for block=%"PRIu32"", block);
        return ret;
    }
    if (is_bad) {
        ESP_LOGD(TAG, "skip erase of bad block=%"PRIu32"", block);
        return ESP_OK;
    }
    ret = nand_erase_block(handle, block);
    return ret;
}

esp_err_t nand_erase_chip(spi_nand_flash_device_t *handle)
{
    esp_err_t ret = ESP_OK;
    for (int i = 0; i < handle->chip.num_blocks; i++) {
        esp_err_t err = nand_erase_good_block(handle, (uint32_t)i);
        if (err == ESP_ERR_NOT_FINISHED) {
            ret = ESP_ERR_NOT_FINISHED;
        } else if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error in nand_erase_chip %d", err);
            return err;
        }
    }
    return ret;
}

esp_err_t nand_prog(spi_nand_flash_device_t *handle, uint32_t page, const uint8_t *data)
{
    ESP_LOGV(TAG, "prog, page=%"PRIu32",", page);
    esp_err_t ret = ESP_OK;
    uint8_t status;

    uint32_t block = page >> handle->chip.log2_ppb;
    uint16_t column_addr = get_column_address(handle, block, 0);

    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, page, NULL), fail, TAG, "");
    ESP_GOTO_ON_ERROR(spi_nand_write_enable(handle), fail, TAG, "");
    ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, data, column_addr, handle->chip.page_size),
                      fail, TAG, "");
#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT
    // Markers layout: BBM from layout + PAGE_USED via FREE_ECC scatter (proposal default §1.2).
    uint8_t *scratch = handle->temp_buffer;
    memset(scratch, 0, 4);
    memcpy(scratch + handle->oob_layout->bbm.bbm_offset, handle->oob_layout->bbm.good_pattern, handle->oob_layout->bbm.bbm_length);
    spi_nand_oob_xfer_ctx_t ctx;
    ESP_GOTO_ON_ERROR(nand_oob_xfer_ctx_init(&ctx, handle->oob_layout, handle, SPI_NAND_OOB_CLASS_FREE_ECC, scratch, 4),
                      fail, TAG, "");
    const uint8_t page_used_prog[2] = { 0x00, 0x00 };
    ESP_GOTO_ON_ERROR(nand_oob_scatter(&ctx, 0, page_used_prog, 2), fail, TAG, "");
    ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, scratch, column_addr + handle->chip.page_size, 4), fail, TAG, "");
#else
    // Markers layout: [bad_block_marker (bytes 0-1)][page_used_marker (bytes 2-3)]
    // For good block with used page: bad=0xFFFF, used=0x0000
    uint8_t markers[4] = { 0xFF, 0xFF, 0x00, 0x00 };
    // Write 4 bytes: bad block marker (0xFFFF - good block) + page used marker (0x0000 - used)
    ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, (uint8_t *)&markers,
                                            column_addr + handle->chip.page_size, 4), fail, TAG, "");
#endif

    ESP_GOTO_ON_ERROR(program_execute_and_wait(handle, page, &status), fail, TAG, "");

    if ((status & STAT_PROGRAM_FAILED) != 0) {
        ESP_LOGE(TAG, "prog failed, page=%"PRIu32", status=0x%02x", page, status);
        return ESP_ERR_NOT_FINISHED;
    }

#if CONFIG_NAND_FLASH_VERIFY_WRITE
    ret = s_verify_write(handle, data, column_addr, handle->chip.page_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s: prog page=%"PRIu32" write verification failed", __func__, page);
    }
#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT
    ret = s_verify_write(handle, handle->temp_buffer, column_addr + handle->chip.page_size, 4);
#else
    uint8_t markers[4] = { 0xFF, 0xFF, 0x00, 0x00 };
    ret = s_verify_write(handle, (uint8_t *)&markers, column_addr + handle->chip.page_size, 4);
#endif
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s: prog page=%"PRIu32" markers write verification failed", __func__, page);
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

    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, page, NULL), fail, TAG, "");

    uint32_t block = page >> handle->chip.log2_ppb;
    uint16_t column_addr = get_column_address(handle, block, handle->chip.page_size);

    // Read 4 bytes to get both bad block marker and page used marker
    ESP_GOTO_ON_ERROR(spi_nand_read(handle, (uint8_t *)handle->read_buffer,
                                    column_addr, 4), fail, TAG, "");

#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT
    spi_nand_oob_xfer_ctx_t ctx;
    ESP_GOTO_ON_ERROR(nand_oob_xfer_ctx_init(&ctx, handle->oob_layout, handle, SPI_NAND_OOB_CLASS_FREE_ECC, handle->read_buffer, 4),
                      fail, TAG, "");
    uint8_t page_used[2];
    ESP_GOTO_ON_ERROR(nand_oob_gather(&ctx, 0, page_used, 2), fail, TAG, "");
    ESP_LOGD(TAG, "is free, page=%"PRIu32", used_marker=%02x,%02x,", page, page_used[0], page_used[1]);
    *is_free_status = (page_used[0] == 0xFF && page_used[1] == 0xFF);
#else
    uint8_t markers[4];
    memcpy(&markers, handle->read_buffer, sizeof(markers));
    ESP_LOGD(TAG, "is free, page=%"PRIu32", used_marker=%02x,%02x,", page, markers[2], markers[3]);
    *is_free_status = (markers[2] == 0xFF && markers[3] == 0xFF);
#endif
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
    nand_ecc_status_t bits_corrected_status = NAND_ECC_OK;
    if (dev->chip.ecc_data.ecc_status_reg_len_in_bits == 2) {
        bits_corrected_status = PACK_2BITS_STATUS(status, STAT_ECC1, STAT_ECC0);
    } else if (dev->chip.ecc_data.ecc_status_reg_len_in_bits == 3) {
        bits_corrected_status = PACK_3BITS_STATUS(status, STAT_ECC2, STAT_ECC1, STAT_ECC0);
    } else {
        bits_corrected_status = NAND_ECC_MAX;
    }
    dev->chip.ecc_data.ecc_corrected_bits_status = bits_corrected_status;
    if (bits_corrected_status) {
        if (bits_corrected_status == NAND_ECC_MAX) {
            ESP_LOGE(TAG, "%s: Error while initializing value of ecc_status_reg_len_in_bits", __func__);
            is_ecc_err = true;
        } else if (bits_corrected_status == NAND_ECC_NOT_CORRECTED) {
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

    uint8_t *cross_copy_buf = NULL;
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
#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT
        uint16_t oob_sz = nand_impl_effective_oob_size(handle);
        ESP_GOTO_ON_FALSE(oob_sz >= 4 && oob_sz <= 2048, ESP_ERR_INVALID_SIZE, fail, TAG, "OOB size");
        size_t copy_alloc = (size_t)handle->chip.page_size + oob_sz;
#else
        size_t copy_alloc = handle->chip.page_size;
#endif
        cross_copy_buf = heap_caps_malloc(copy_alloc, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        ESP_GOTO_ON_FALSE(cross_copy_buf, ESP_ERR_NO_MEM, fail, TAG, "Failed to allocate copy buffer");

        ESP_GOTO_ON_ERROR(spi_nand_read(handle, cross_copy_buf, src_column_addr, handle->chip.page_size), fail, TAG, "");
#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT
        ESP_GOTO_ON_ERROR(spi_nand_read(handle, cross_copy_buf + handle->chip.page_size, src_column_addr + handle->chip.page_size, oob_sz),
                          fail, TAG, "");
#else
        const uint8_t markers_legacy[4] = { 0xFF, 0xFF, 0x00, 0x00 };
        memcpy(handle->temp_buffer, markers_legacy, 4);
#endif

        ESP_GOTO_ON_ERROR(spi_nand_write_enable(handle), fail, TAG, "");

        ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, cross_copy_buf, dst_column_addr, handle->chip.page_size),
                          fail, TAG, "");
#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT
        ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, cross_copy_buf + handle->chip.page_size, dst_column_addr + handle->chip.page_size, oob_sz),
                          fail, TAG, "");
#else
        // Write 4 bytes: bad block marker (0xFFFF - good block) + page used marker (0x0000 - used)
        ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, handle->temp_buffer, dst_column_addr + handle->chip.page_size, 4),
                          fail, TAG, "");
#endif
        ESP_GOTO_ON_ERROR(program_execute_and_wait(handle, dst, &status), fail, TAG, "");

        if ((status & STAT_PROGRAM_FAILED) != 0) {
            ESP_LOGD(TAG, "copy, prog failed");
            free(cross_copy_buf);
            return ESP_ERR_NOT_FINISHED;
        }
        free(cross_copy_buf);
        cross_copy_buf = NULL;
    } else {
        // Proposal §7.0: same-plane internal page program carries full spare from chip cache — no OOB program_load here.
        ESP_GOTO_ON_ERROR(program_execute_and_wait(handle, dst, &status), fail, TAG, "");
        if ((status & STAT_PROGRAM_FAILED) != 0) {
            ESP_LOGD(TAG, "copy, prog failed");
            return ESP_ERR_NOT_FINISHED;
        }
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
    free(cross_copy_buf);
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
