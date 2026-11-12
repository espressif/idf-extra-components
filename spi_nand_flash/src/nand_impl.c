/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2026 Espressif Systems (Shanghai) CO LTD
 */

#include <string.h>
#include <inttypes.h>
#include "esp_check.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "spi_nand_oper.h"
#include "nand.h"
#include "nand_flash_devices.h"
#include "nand_device_types.h"
#include "nand_ecc_decode.h"

#define ROM_WAIT_THRESHOLD_US 1000
/* Vendor tables pass typical tR/tPROG/tBERS; datasheet max is often several
 * times that. Bound hangs at 10× typical plus one tick so max/tick rounding
 * cannot false-timeout. Wrong if actual BUSY exceeds 10× the delay passed in. */
#define NAND_WAIT_READY_TIMEOUT_MULT 10U

/* spi_nand_oper.c bounces non-DMA buffers through temp_buffer only on IDF >= 5.2 */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
#define NAND_PAGE_BUF_CAPS (MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT)
#else
#define NAND_PAGE_BUF_CAPS (MALLOC_CAP_DMA | MALLOC_CAP_8BIT)
#endif

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
    case SPI_NAND_FLASH_FM_MI: // FM
        return spi_nand_fm_init(dev);
    case SPI_NAND_FLASH_MACRONIX_MI: // Macronix
        return spi_nand_macronix_init(dev);
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

    *handle = heap_caps_calloc(1, sizeof(spi_nand_flash_device_t), MALLOC_CAP_DEFAULT);
    if (*handle == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memcpy(&(*handle)->config, config, sizeof(spi_nand_flash_config_t));

    (*handle)->chip.ecc_data.ecc_data_refresh_threshold = 4;
    (*handle)->chip.ecc_data.ecc_status_reg_len_in_bits = 2;
    (*handle)->ecc_status_decoder = nand_ecc_decode_2bit;
    (*handle)->chip.log2_ppb = 6;         // 64 pages per block is standard
    (*handle)->chip.log2_page_size = 11;  // 2048 bytes per page is fairly standard
    (*handle)->chip.num_planes = 1;
    (*handle)->chip.num_luns_per_target = 1;
    (*handle)->chip.flags = 0;

    ESP_GOTO_ON_ERROR(detect_chip(*handle), fail, TAG, "Failed to detect nand chip");
    ESP_GOTO_ON_FALSE((*handle)->chip.num_luns_per_target != 0 &&
                      (*handle)->chip.num_blocks % (*handle)->chip.num_luns_per_target == 0,
                      ESP_ERR_INVALID_STATE, fail, TAG, "num_blocks not divisible by num_luns_per_target");
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

static esp_err_t wait_for_ready(spi_nand_flash_device_t *dev, uint32_t expected_operation_time_us, uint8_t *status_out)
{
    if (expected_operation_time_us < ROM_WAIT_THRESHOLD_US) {
        esp_rom_delay_us(expected_operation_time_us);
    }

    /* Hang bound only. Success path still polls until BUSY clears. */
    uint64_t timeout_us = (uint64_t)expected_operation_time_us * NAND_WAIT_READY_TIMEOUT_MULT;
    uint32_t timeout_ms = (uint32_t)((timeout_us + 999U) / 1000U);
    if (timeout_ms == 0) {
        timeout_ms = 1;
    }
    TickType_t timeout_ticks = (timeout_ms + portTICK_PERIOD_MS - 1U) / portTICK_PERIOD_MS;
    timeout_ticks += 1;
    TickType_t start_tick = xTaskGetTickCount();

    while (true) {
        uint8_t status;
        ESP_RETURN_ON_ERROR(spi_nand_read_register(dev, REG_STATUS, &status), TAG, "");

        if ((status & STAT_BUSY) == 0) {
            if (status_out) {
                *status_out = status;
            }
            break;
        }

        if ((xTaskGetTickCount() - start_tick) >= timeout_ticks) {
            ESP_LOGE(TAG, "STAT_BUSY timeout: status=0x%02x, expected_op=%" PRIu32 " us, waited=%" PRIu32 " ms",
                     status, expected_operation_time_us,
                     (uint32_t)timeout_ticks * portTICK_PERIOD_MS);
            return ESP_ERR_TIMEOUT;
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

/* Once Program Execute is issued the page may be programmed, so any failure from then on
 * is reported as ESP_ERR_NOT_FINISHED and the page must not be programmed again. */
static esp_err_t program_execute_and_wait(spi_nand_flash_device_t *dev, uint32_t page)
{
    uint8_t status;
    ESP_RETURN_ON_FALSE(spi_nand_program_execute(dev, page) == ESP_OK, ESP_ERR_NOT_FINISHED, TAG, "");
    ESP_RETURN_ON_FALSE(wait_for_ready(dev, dev->chip.program_page_delay_us, &status) == ESP_OK,
                        ESP_ERR_NOT_FINISHED, TAG, "");
    ESP_RETURN_ON_FALSE((status & STAT_PROGRAM_FAILED) == 0, ESP_ERR_NOT_FINISHED, TAG,
                        "prog failed, page=%"PRIu32", status=0x%02x", page, status);
    return ESP_OK;
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
        // Plane index is encoded in the column address bit after the page MSB.
        // 2048-byte pages use bit 12; 4096-byte pages use bit 13.
        column_addr += plane << (handle->chip.log2_page_size + 1);
    }
    return column_addr;
}

esp_err_t nand_is_bad(spi_nand_flash_device_t *handle, uint32_t block, bool *is_bad_status)
{
    uint32_t first_block_page = block * (1 << handle->chip.log2_ppb);
    // Markers layout: [bad_block_marker (bytes 0-1)][page_used_marker (bytes 2-3)]
    uint8_t markers[4];
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, first_block_page, NULL), fail, TAG, "");

    uint16_t column_addr = get_column_address(handle, block, handle->chip.page_size);

    // Read 4 bytes to include both bad block marker and page status
    ESP_GOTO_ON_ERROR(spi_nand_read(handle, (uint8_t *) handle->read_buffer, column_addr, 4),
                      fail, TAG, "");

    memcpy(&markers, handle->read_buffer, sizeof(markers));
    ESP_LOGV(TAG, "is_bad, block=%"PRIu32", page=%"PRIu32",indicator = %02x,%02x", block, first_block_page, markers[0], markers[1]);
    *is_bad_status = (markers[0] != 0xFF || markers[1] != 0xFF);
    return ret;

fail:
    ESP_LOGE(TAG, "Error in nand_is_bad %d", ret);
    return ret;
}

esp_err_t nand_mark_bad(spi_nand_flash_device_t *handle, uint32_t block)
{
    esp_err_t ret = ESP_OK;

    uint32_t first_block_page = block * (1 << handle->chip.log2_ppb);
    // Markers layout: [bad_block_marker (bytes 0-1)][page_used_marker (bytes 2-3)]
    const uint8_t markers[4] = { 0x00, 0x00, 0xFF, 0xFF }; //// 0x0000 (bad block), 0xFFFF (free)
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

    // Write 4 bytes: bad block marker (0x0000) + page used marker (0xFFFF)
    ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, (const uint8_t *) &markers,
                                            column_addr, 4), fail, TAG, "");
    ESP_GOTO_ON_ERROR(program_execute_and_wait(handle, first_block_page), fail, TAG, "");

#if CONFIG_NAND_FLASH_VERIFY_WRITE
    // nand_is_bad() reloads the page and checks the bad block marker; the other bytes do not matter for a bad block
    bool is_bad = false;
    ESP_GOTO_ON_ERROR(nand_is_bad(handle, block, &is_bad), fail, TAG, "");
    ESP_GOTO_ON_FALSE(is_bad, ESP_FAIL, fail, TAG, "mark_bad verification failed, block=%"PRIu32, block);
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE
    return ret;
fail:
    ESP_LOGE(TAG, "Error in nand_mark_bad %d", ret);
    return ret;
}

esp_err_t nand_erase_block(spi_nand_flash_device_t *handle, uint32_t block)
{
    ESP_LOGV(TAG, "erase_block, block=%"PRIu32",", block);
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
    ESP_LOGV(TAG, "erase_block, block=%"PRIu32",", block);
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

#if CONFIG_NAND_FLASH_VERIFY_WRITE
static esp_err_t check_ecc_status(spi_nand_flash_device_t *dev, uint8_t status);

/* Reloads the programmed page from the array and compares data and markers with what was written.
 * Any failure is ESP_ERR_NOT_FINISHED since the page has been programmed. buf must hold a page. */
static esp_err_t s_verify_page(spi_nand_flash_device_t *handle, uint32_t page, uint16_t column_addr,
                               const uint8_t *data, const uint8_t *markers, uint8_t *buf)
{
    uint8_t status;
    uint8_t read_markers[4];
    ESP_RETURN_ON_FALSE(read_page_and_wait(handle, page, &status) == ESP_OK, ESP_ERR_NOT_FINISHED, TAG,
                        "verify: failed to reload page=%"PRIu32, page);
    ESP_RETURN_ON_FALSE(check_ecc_status(handle, status) == ESP_OK, ESP_ERR_NOT_FINISHED, TAG,
                        "verify: page=%"PRIu32" read, ecc error", page);
    ESP_RETURN_ON_FALSE(spi_nand_read(handle, buf, column_addr, handle->chip.page_size) == ESP_OK,
                        ESP_ERR_NOT_FINISHED, TAG, "verify: failed to read data of page=%"PRIu32, page);
    ESP_RETURN_ON_FALSE(memcmp(buf, data, handle->chip.page_size) == 0, ESP_ERR_NOT_FINISHED, TAG,
                        "verify: data mismatch, page=%"PRIu32, page);
    ESP_RETURN_ON_FALSE(spi_nand_read(handle, read_markers, column_addr + handle->chip.page_size,
                                      sizeof(read_markers)) == ESP_OK,
                        ESP_ERR_NOT_FINISHED, TAG, "verify: failed to read markers of page=%"PRIu32, page);
    ESP_RETURN_ON_FALSE(memcmp(read_markers, markers, sizeof(read_markers)) == 0, ESP_ERR_NOT_FINISHED, TAG,
                        "verify: markers mismatch, page=%"PRIu32, page);
    return ESP_OK;
}
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE

esp_err_t nand_prog(spi_nand_flash_device_t *handle, uint32_t page, const uint8_t *data)
{
    ESP_LOGV(TAG, "prog, page=%"PRIu32",", page);
    esp_err_t ret = ESP_OK;
    // Markers layout: [bad_block_marker (bytes 0-1)][page_used_marker (bytes 2-3)]
    // For good block with used page: bad=0xFFFF, used=0x0000
    uint8_t markers[4] = { 0xFF, 0xFF, 0x00, 0x00 };
#if CONFIG_NAND_FLASH_VERIFY_WRITE
    uint8_t *verify_buf = NULL;
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE

    uint32_t block = page >> handle->chip.log2_ppb;
    uint16_t column_addr = get_column_address(handle, block, 0);

    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, page, NULL), fail, TAG, "");
#if CONFIG_NAND_FLASH_VERIFY_WRITE
    verify_buf = heap_caps_malloc(handle->chip.page_size, NAND_PAGE_BUF_CAPS);
    ESP_GOTO_ON_FALSE(verify_buf != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE
    ESP_GOTO_ON_ERROR(spi_nand_write_enable(handle), fail, TAG, "");
    ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, data, column_addr, handle->chip.page_size),
                      fail, TAG, "");
    // Write 4 bytes: bad block marker (0xFFFF - good block) + page used marker (0x0000 - used)
    ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, (uint8_t *)&markers,
                                            column_addr + handle->chip.page_size, 4), fail, TAG, "");

    ESP_GOTO_ON_ERROR(program_execute_and_wait(handle, page), fail, TAG, "");

#if CONFIG_NAND_FLASH_VERIFY_WRITE
    ESP_GOTO_ON_ERROR(s_verify_page(handle, page, column_addr, data, markers, verify_buf), fail, TAG,
                      "prog page=%"PRIu32" write verification failed", page);
    free(verify_buf);
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE

    return ret;
fail:
#if CONFIG_NAND_FLASH_VERIFY_WRITE
    free(verify_buf);
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE
    ESP_LOGE(TAG, "Error in nand_prog %d", ret);
    return ret;
}

esp_err_t nand_is_free(spi_nand_flash_device_t *handle, uint32_t page, bool *is_free_status)
{
    esp_err_t ret = ESP_OK;
    // Markers layout: [bad_block_marker (bytes 0-1)][page_used_marker (bytes 2-3)]
    uint8_t markers[4];

    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, page, NULL), fail, TAG, "");

    uint32_t block = page >> handle->chip.log2_ppb;
    uint16_t column_addr = get_column_address(handle, block, handle->chip.page_size);

    // Read 4 bytes to get both bad block marker and page used marker
    ESP_GOTO_ON_ERROR(spi_nand_read(handle, (uint8_t *)handle->read_buffer,
                                    column_addr, 4), fail, TAG, "");

    memcpy(&markers, handle->read_buffer, sizeof(markers));
    ESP_LOGV(TAG, "is free, page=%"PRIu32", used_marker=%02x,%02x,", page, markers[2], markers[3]);
    *is_free_status = (markers[2] == 0xFF && markers[3] == 0xFF);
    return ret;
fail:
    ESP_LOGE(TAG, "Error in nand_is_free %d", ret);
    return ret;
}

/* Decode the ECC status for the page just read and record it.
 * Returns ESP_OK if the data is usable, ESP_FAIL on an uncorrectable/invalid ECC status,
 * or the decoder's error (e.g. a failed status-extension register read) unchanged.
 * dhara_glue.c relies on ESP_FAIL from nand_read()/nand_copy() meaning exactly this ECC verdict. */
static esp_err_t check_ecc_status(spi_nand_flash_device_t *dev, uint8_t status)
{
    assert(dev->ecc_status_decoder);
    nand_ecc_status_t bits_corrected_status = NAND_ECC_INVALID;
    esp_err_t ret = dev->ecc_status_decoder(dev, status, &bits_corrected_status);
    if (ret != ESP_OK) {
        /* Status unknown: not an ECC verdict, so don't report NOT_CORRECTED. */
        dev->chip.ecc_data.ecc_corrected_bits_status = NAND_ECC_INVALID;
        return ret;
    }
    dev->chip.ecc_data.ecc_corrected_bits_status = bits_corrected_status;
    /* NAND_ECC_MAX is not a status; a decoder returning it is a bug, so fail safe. */
    if (bits_corrected_status == NAND_ECC_NOT_CORRECTED || bits_corrected_status == NAND_ECC_INVALID ||
            bits_corrected_status >= NAND_ECC_MAX) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t nand_read(spi_nand_flash_device_t *handle, uint32_t page, size_t offset, size_t length, uint8_t *data)
{
    ESP_LOGV(TAG, "read, page=%"PRIu32", offset=%d, length=%d", page, offset, length);
    assert(page < handle->chip.num_blocks * (1 << handle->chip.log2_ppb));
    esp_err_t ret = ESP_OK;
    uint8_t status;

    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, page, &status), fail, TAG, "");

    ret = check_ecc_status(handle, status);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "read ecc error, page=%"PRIu32"", page);
        return ret;
    }

    uint32_t block = page >> handle->chip.log2_ppb;
    uint16_t column_addr = get_column_address(handle, block, offset);

    ESP_GOTO_ON_ERROR(spi_nand_read(handle, data, column_addr, length), fail, TAG, "");

    return ret;
fail:
    ESP_LOGE(TAG, "Error in nand_read %d", ret);
    return ret;
}

static bool nand_copy_needs_ram_path(const spi_nand_chip_t *chip,
                                     uint32_t src_block,
                                     uint32_t dst_block,
                                     uint16_t src_column_addr,
                                     uint16_t dst_column_addr)
{
    if (src_column_addr != dst_column_addr) {
        return true;
    }
    // Each LUN has its own cache register, so Internal Data Move cannot cross LUNs
    if (chip->num_luns_per_target > 1) {
        uint32_t blocks_per_lun = chip->num_blocks / chip->num_luns_per_target;
        if (src_block / blocks_per_lun != dst_block / blocks_per_lun) {
            return true;
        }
    }
    return (chip->flags & NAND_FLAG_IDM_SAME_PARITY_REQUIRED) &&
           ((src_block & 1U) != (dst_block & 1U));
}

esp_err_t nand_copy(spi_nand_flash_device_t *handle, uint32_t src, uint32_t dst)
{
    ESP_LOGD(TAG, "copy, src=%"PRIu32", dst=%"PRIu32"", src, dst);
    esp_err_t ret = ESP_OK;
    uint8_t *copy_buf = NULL;
    // Markers layout: [bad_block_marker (bytes 0-1)][page_used_marker (bytes 2-3)]
    // For good block with used page: bad=0xFFFF, used=0x0000
    uint8_t markers[4] = { 0xFF, 0xFF, 0x00, 0x00 };
#if CONFIG_NAND_FLASH_VERIFY_WRITE
    uint8_t *verify_buf = NULL;
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE

    uint8_t status;
    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, src, &status), fail, TAG, "");

    ret = check_ecc_status(handle, status);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "copy, ecc error");
        return ret;
    }

    uint32_t src_block = src >> handle->chip.log2_ppb;
    uint32_t dst_block = dst >> handle->chip.log2_ppb;
    uint16_t src_column_addr = get_column_address(handle, src_block, 0);
    uint16_t dst_column_addr = get_column_address(handle, dst_block, 0);

    // Copy through RAM when HW Internal Data Move is not valid for this src/dst pair.
    bool need_ram_copy = nand_copy_needs_ram_path(&handle->chip, src_block, dst_block,
                                                  src_column_addr, dst_column_addr);
    bool need_copy_buf = need_ram_copy;
#if CONFIG_NAND_FLASH_VERIFY_WRITE
    need_copy_buf = true;
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE

    // Allocate before Program Execute so an allocation failure leaves dst untouched
    if (need_copy_buf) {
        copy_buf = heap_caps_malloc(handle->chip.page_size, NAND_PAGE_BUF_CAPS);
        ESP_GOTO_ON_FALSE(copy_buf, ESP_ERR_NO_MEM, fail, TAG, "Failed to allocate copy buffer");
        // The cache is invalid after Program Execute, so capture src now (reference for verify)
        ESP_GOTO_ON_ERROR(spi_nand_read(handle, copy_buf, src_column_addr, handle->chip.page_size), fail, TAG, "");
    }
#if CONFIG_NAND_FLASH_VERIFY_WRITE
    verify_buf = heap_caps_malloc(handle->chip.page_size, NAND_PAGE_BUF_CAPS);
    ESP_GOTO_ON_FALSE(verify_buf != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");
    if (!need_ram_copy) {
        // Internal Data Move programs the markers of src
        ESP_GOTO_ON_ERROR(spi_nand_read(handle, markers, src_column_addr + handle->chip.page_size, sizeof(markers)),
                          fail, TAG, "");
    }
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE

    ESP_GOTO_ON_ERROR(spi_nand_write_enable(handle), fail, TAG, "");

    if (need_ram_copy) {
        ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, copy_buf, dst_column_addr, handle->chip.page_size),
                          fail, TAG, "");
        ESP_GOTO_ON_ERROR(spi_nand_program_load(handle, markers,
                                                dst_column_addr + handle->chip.page_size, sizeof(markers)),
                          fail, TAG, "");
    }

    ESP_GOTO_ON_ERROR(program_execute_and_wait(handle, dst), fail, TAG, "");

#if CONFIG_NAND_FLASH_VERIFY_WRITE
    ESP_GOTO_ON_ERROR(s_verify_page(handle, dst, dst_column_addr, copy_buf, markers, verify_buf), fail, TAG,
                      "copy src_page=%"PRIu32" dst_page=%"PRIu32" (%s) write verification failed", src, dst,
                      need_ram_copy ? "via RAM" : "internal data move");
    free(verify_buf);
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE
    free(copy_buf);
    return ret;

fail:
#if CONFIG_NAND_FLASH_VERIFY_WRITE
    free(verify_buf);
#endif //CONFIG_NAND_FLASH_VERIFY_WRITE
    free(copy_buf);
    ESP_LOGE(TAG, "Error in nand_copy %d", ret);
    return ret;
}

esp_err_t nand_get_ecc_status(spi_nand_flash_device_t *handle, uint32_t page)
{
    esp_err_t ret = ESP_OK;
    uint8_t status;
    ESP_GOTO_ON_ERROR(read_page_and_wait(handle, page, &status), fail, TAG, "");

    esp_err_t ecc_ret = check_ecc_status(handle, status);
    if (ecc_ret == ESP_FAIL) {
        /* ECC verdict is reported via ecc_corrected_bits_status, not the return code. */
        ESP_LOGD(TAG, "read ecc error, page=%"PRIu32"", page);
    } else if (ecc_ret != ESP_OK) {
        ret = ecc_ret;
        goto fail;
    }
    return ret;

fail:
    ESP_LOGE(TAG, "Error in nand_get_ecc_status %d", ret);
    return ret;
}
