/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nand.h"
#include "nand_impl.h"
#include "nand_flash_devices.h"
#include "esp_nand_blockdev.h"
#include "nand_device_types.h"

#ifndef CONFIG_IDF_TARGET_LINUX
#include "spi_nand_oper.h"
#endif

static const char *TAG = "nand_flash_blockdev";

#ifdef CONFIG_IDF_TARGET_LINUX
static esp_err_t detect_chip(spi_nand_flash_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    spi_nand_flash_config_t *config = &dev->config;

    ESP_GOTO_ON_ERROR(nand_emul_init(dev, config->emul_conf), fail, TAG, "");
    dev->chip.page_size = (1 << dev->chip.log2_page_size);

    dev->chip.emulated_page_oob = 64;  // The default page size is 2048, so the OOB size is 64.

    if (dev->chip.page_size == 512) {
        dev->chip.emulated_page_oob = 16;
    } else if (dev->chip.page_size == 2048) {
        dev->chip.emulated_page_oob = 64;
    } else if (dev->chip.page_size == 4096) {
        dev->chip.emulated_page_oob = 128;
    }
    dev->chip.emulated_page_size = dev->chip.page_size + dev->chip.emulated_page_oob;
    dev->chip.block_size = (1 << dev->chip.log2_ppb) * dev->chip.emulated_page_size;

    if (dev->chip.block_size == 0) {
        ESP_LOGE(TAG, "Invalid block size (0)");
        ret = ESP_ERR_INVALID_SIZE;
        goto fail;
    }

    dev->chip.num_blocks = config->emul_conf->flash_file_size / dev->chip.block_size;
    dev->chip.erase_block_delay_us = 3000;
    dev->chip.program_page_delay_us = 630;
    dev->chip.read_page_delay_us = 60;
fail:
    return ret;
}

static esp_err_t enable_quad_io_mode(spi_nand_flash_device_t *dev)
{
    return ESP_OK;
}

static esp_err_t unprotect_chip(spi_nand_flash_device_t *dev)
{
    return ESP_OK;
}

#else

static esp_err_t detect_chip(spi_nand_flash_device_t *dev)
{
    uint8_t manufacturer_id;
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
#endif //CONFIG_IDF_TARGET_LINUX

esp_err_t nand_init_device(spi_nand_flash_config_t *config, spi_nand_flash_device_t **handle)
{
    esp_err_t ret = ESP_OK;
#ifdef CONFIG_IDF_TARGET_LINUX
    ESP_RETURN_ON_FALSE(config->emul_conf != NULL, ESP_ERR_INVALID_ARG, TAG, "Linux mmap emulation configuration pointer can not be NULL");
#else
    ESP_RETURN_ON_FALSE(config->device_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Spi device pointer can not be NULL");
#endif //CONFIG_IDF_TARGET_LINUX

    *handle = heap_caps_calloc(1, sizeof(spi_nand_flash_device_t), MALLOC_CAP_DEFAULT);
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

    (*handle)->work_buffer = heap_caps_malloc((*handle)->chip.page_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE((*handle)->work_buffer != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");

    (*handle)->read_buffer = heap_caps_malloc((*handle)->chip.page_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE((*handle)->read_buffer != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");

    (*handle)->temp_buffer = heap_caps_malloc((*handle)->chip.page_size + 1, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
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

/**************************************************************************************
 **************************************************************************************
 * Block Device Layer interface implementation
 **************************************************************************************
 */

static esp_err_t nand_flash_blockdev_read(esp_blockdev_handle_t handle, uint8_t *dst_buf, size_t dst_buf_size, uint64_t src_addr, size_t data_read_len)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->geometry.read_size == 0) {
        ESP_LOGE(TAG, "Invalid read size (0)");
        return ESP_ERR_INVALID_SIZE;
    }

    // Note: Read operations can have offsets, so we don't enforce strict page alignment
    // But we validate the address + length doesn't exceed page boundaries inappropriately
    if (data_read_len > handle->geometry.read_size) {
        ESP_LOGE(TAG, "Read length %u exceeds page size %u", data_read_len, handle->geometry.read_size);
        return ESP_ERR_INVALID_SIZE;
    }

    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)handle->ctx;
    uint32_t page = src_addr / handle->geometry.read_size;
    size_t offset = src_addr % handle->geometry.read_size;

    // Ensure read doesn't cross page boundary
    if (offset + data_read_len > handle->geometry.read_size) {
        ESP_LOGE(TAG, "Read crosses page boundary: offset=%u + len=%u > page_size=%u",
                 offset, data_read_len, handle->geometry.read_size);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t res = nand_read(dev_handle, page, offset, data_read_len, dst_buf);
    return res;
}

static esp_err_t nand_flash_blockdev_write(esp_blockdev_handle_t handle, const uint8_t *src_buf, uint64_t dst_addr, size_t data_write_len)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->geometry.write_size == 0) {
        ESP_LOGE(TAG, "Invalid write size (0)");
        return ESP_ERR_INVALID_SIZE;
    }

    // NAND flash requires page-aligned writes
    if ((dst_addr % handle->geometry.write_size) != 0) {
        ESP_LOGE(TAG, "Write address 0x%llx not aligned to page size %u", dst_addr, handle->geometry.write_size);
        return ESP_ERR_INVALID_ARG;
    }
    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)handle->ctx;
    uint32_t page = dst_addr / handle->geometry.write_size;
    esp_err_t res = nand_prog(dev_handle, page, src_buf);
    return res;
}

static esp_err_t nand_flash_blockdev_erase(esp_blockdev_handle_t handle, uint64_t start_addr, size_t erase_len)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->geometry.erase_size == 0) {
        ESP_LOGE(TAG, "Invalid erase size (0)");
        return ESP_ERR_INVALID_SIZE;
    }

    // NAND flash requires block-aligned erases
    if ((start_addr % handle->geometry.erase_size) != 0) {
        ESP_LOGE(TAG, "Erase address 0x%llx not aligned to block size %u", start_addr, handle->geometry.erase_size);
        return ESP_ERR_INVALID_ARG;
    }

    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)handle->ctx;
    uint32_t block = start_addr / handle->geometry.erase_size;
    esp_err_t res = nand_erase_block(dev_handle, block);
    return res;
}

static esp_err_t nand_flash_blockdev_sync_no_op(esp_blockdev_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t res = ESP_OK;
    return res;
}

static bool is_ecc_exceed_threshold(spi_nand_flash_device_t *handle)
{
    uint8_t min_bits_corrected = 0;
    bool ret = false;
    if (handle->chip.ecc_data.ecc_corrected_bits_status == NAND_ECC_1_TO_3_BITS_CORRECTED) {
        min_bits_corrected = 1;
    } else if (handle->chip.ecc_data.ecc_corrected_bits_status == NAND_ECC_4_TO_6_BITS_CORRECTED) {
        min_bits_corrected = 4;
    } else if (handle->chip.ecc_data.ecc_corrected_bits_status == NAND_ECC_7_8_BITS_CORRECTED) {
        min_bits_corrected = 7;
    }

    if (min_bits_corrected >= handle->chip.ecc_data.ecc_data_refresh_threshold) {
        ret = true;
    }
    return ret;
}

static esp_err_t nand_flash_blockdev_ioctl(esp_blockdev_handle_t handle, const uint8_t cmd, void *args)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    spi_nand_flash_device_t *dev = (spi_nand_flash_device_t *)handle->ctx;

    switch (cmd) {
    case ESP_BLOCKDEV_CMD_IS_BAD_BLOCK: {
        esp_blockdev_cmd_arg_is_bad_block_t *bad_block_status = (esp_blockdev_cmd_arg_is_bad_block_t *) args;
        esp_err_t ret = nand_is_bad(dev, bad_block_status->num, &bad_block_status->status);
        return ret;
    }

    case ESP_BLOCKDEV_CMD_MARK_BAD_BLOCK: {
        uint32_t *block = (uint32_t *) args;
        esp_err_t ret = nand_mark_bad(dev, *block);
        return ret;
    }

    case ESP_BLOCKDEV_CMD_IS_FREE_PAGE: {
        esp_blockdev_cmd_arg_is_free_page_t *page_free_status = (esp_blockdev_cmd_arg_is_free_page_t *) args;
        esp_err_t ret = nand_is_free(dev, page_free_status->num, &page_free_status->status);
        return ret;
    }

    case ESP_BLOCKDEV_CMD_GET_PAGE_ECC_STATUS: {
        esp_blockdev_cmd_arg_ecc_status_t *page_ecc_status = (esp_blockdev_cmd_arg_ecc_status_t *) args;
        esp_err_t ret = nand_get_ecc_status(dev, page_ecc_status->page_num);
        page_ecc_status->ecc_status = dev->chip.ecc_data.ecc_corrected_bits_status;
        return ret;
    }

    case ESP_BLOCKDEV_CMD_GET_NAND_FLASH_INFO: {
        esp_blockdev_cmd_arg_nand_flash_info_t *flash_info = (esp_blockdev_cmd_arg_nand_flash_info_t *)args;
        flash_info->device_info.manufacturer_id = dev->device_info.manufacturer_id;
        flash_info->device_info.device_id = dev->device_info.device_id;
        memcpy(flash_info->device_info.chip_name, dev->device_info.chip_name, sizeof(dev->device_info.chip_name));
        memcpy(&flash_info->geometry, &dev->chip, sizeof(nand_flash_geometry_t));
    }
    break;

    case ESP_BLOCKDEV_CMD_GET_BAD_BLOCKS_COUNT: {
        uint32_t *bad_block_count = (uint32_t *)args;
        uint32_t num_blocks = dev->chip.num_blocks;
        uint32_t bad_blocks = 0;
        for (uint32_t blk = 0; blk < num_blocks; blk++) {
            bool is_bad = false;
            esp_err_t ret = nand_is_bad(dev, blk, &is_bad);
            if (ret == ESP_OK && is_bad) {
                bad_blocks++;
                ESP_LOGD(TAG, "bad block num=%"PRIu32"", blk);
            } else if (ret) {
                ESP_LOGE(TAG, "Failed to get bad block status for blk=%"PRIu32"", blk);
                return ret;
            }
        }
        *bad_block_count = bad_blocks;
    }
    break;

    case ESP_BLOCKDEV_CMD_GET_ECC_STATS: {
        esp_blockdev_cmd_arg_ecc_stats_t *ecc_stats = (esp_blockdev_cmd_arg_ecc_stats_t *)args;
        spi_nand_flash_device_t *flash = (spi_nand_flash_device_t *)handle->ctx;

        if (handle->geometry.write_size == 0) {
            ESP_LOGE(TAG, "Invalid write size (0)");
            return ESP_ERR_INVALID_SIZE;
        }

        uint32_t num_pages = handle->geometry.disk_size / handle->geometry.write_size;
        bool is_free = true;
        uint32_t ecc_err_total_count = 0;
        uint32_t ecc_err_exceeding_threshold_count = 0;
        uint32_t ecc_err_not_corrected_count = 0;
        esp_err_t ret = ESP_OK;
        for (uint32_t page = 0; page < num_pages; page++) {
            ret = nand_is_free(dev, page, &is_free);
            if (ret == ESP_OK && !is_free) {
                xSemaphoreTake(flash->mutex, portMAX_DELAY);
                ret = nand_get_ecc_status(dev, page);
                xSemaphoreGive(flash->mutex);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to read ecc error for page=%" PRIu32 "", page);
                    return ret;
                }
                if (flash->chip.ecc_data.ecc_corrected_bits_status) {
                    ecc_err_total_count++;
                    if (flash->chip.ecc_data.ecc_corrected_bits_status == NAND_ECC_NOT_CORRECTED) {
                        ecc_err_not_corrected_count++;
                        ESP_LOGD(TAG, "ecc error not corrected for page=%" PRIu32 "", page);
                    } else if (is_ecc_exceed_threshold(flash)) {
                        ecc_err_exceeding_threshold_count++;
                    }
                }
            }
        }
        ecc_stats->ecc_threshold = flash->chip.ecc_data.ecc_data_refresh_threshold;
        ecc_stats->ecc_total_err_count = ecc_err_total_count;
        ecc_stats->ecc_uncorreced_err_count = ecc_err_not_corrected_count;
        ecc_stats->ecc_exceeding_threshold_err_count = ecc_err_exceeding_threshold_count;
    }
    break;

    default:
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

static esp_err_t nand_flash_blockdev_release(esp_blockdev_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t res = ESP_OK;
    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)handle->ctx;
    free(dev_handle->work_buffer);
    free(dev_handle->read_buffer);
    free(dev_handle->temp_buffer);
    if (dev_handle->mutex) {
        vSemaphoreDelete(dev_handle->mutex);
    }
    free(dev_handle);
    free(handle);
    return res;
}

static const esp_blockdev_ops_t nand_flash_blockdev_ops = {
    .read = nand_flash_blockdev_read,
    .write = nand_flash_blockdev_write,
    .erase = nand_flash_blockdev_erase,
    .ioctl = nand_flash_blockdev_ioctl,
    .sync = nand_flash_blockdev_sync_no_op,
    .release = nand_flash_blockdev_release,
};

esp_err_t nand_flash_get_blockdev(spi_nand_flash_config_t *config, esp_blockdev_handle_t *out_bdl_handle_ptr)
{
    if (config == NULL || out_bdl_handle_ptr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    spi_nand_flash_device_t *handle = NULL;
    esp_err_t ret = nand_init_device(config, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_blockdev_t *blockdev = (esp_blockdev_t *) heap_caps_calloc(1, sizeof(esp_blockdev_t), MALLOC_CAP_DEFAULT);
    if (blockdev == NULL) {
        return ESP_ERR_NO_MEM;
    }
    blockdev->ctx = (void *)handle;
    *out_bdl_handle_ptr = blockdev;

    blockdev->ops = &nand_flash_blockdev_ops;

    // Set up geometry information
    uint32_t sector_size = handle->chip.page_size;
    uint32_t block_size = handle->chip.block_size;
    uint32_t num_blocks = handle->chip.num_blocks;

    blockdev->geometry.disk_size = num_blocks * block_size;
    blockdev->geometry.write_size = sector_size;
    blockdev->geometry.read_size = sector_size;
    blockdev->geometry.erase_size = block_size;
    blockdev->geometry.recommended_write_size = sector_size;
    blockdev->geometry.recommended_read_size = sector_size;
    blockdev->geometry.recommended_erase_size = block_size;

    return ESP_OK;
}
