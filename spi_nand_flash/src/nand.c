/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2024 Espressif Systems (Shanghai) CO LTD
 */

#include <string.h>
#include "esp_check.h"
#include "spi_nand_flash.h"
#include "nand.h"
#include "esp_blockdev.h"


#ifndef CONFIG_IDF_TARGET_LINUX
#include "spi_nand_oper.h"
#include "nand_impl.h"
#include "nand_flash_devices.h"
#include "nand_flash_chip.h"
#include "esp_vfs_fat_nand.h"
#endif //CONFIG_IDF_TARGET_LINUX

static const char *TAG = "nand_flash";

#ifdef CONFIG_IDF_TARGET_LINUX

static esp_err_t detect_chip(spi_nand_flash_device_t *dev, spi_nand_flash_config_t *config)
{
    esp_err_t ret = ESP_OK;

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
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .address = 0, // This normally selects the manufacturer id. Some chips ignores it, but still expects 8 dummy bits here
        .address_bytes = 1,
        .miso_len = 1,
        .miso_data = &manufacturer_id,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    spi_nand_execute_transaction(dev, &t);
    ESP_LOGD(TAG, "%s: manufacturer_id: %x\n", __func__, manufacturer_id);

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

esp_err_t spi_nand_flash_init_device(spi_nand_flash_config_t *config, spi_nand_flash_device_t **handle)
{
#ifdef CONFIG_IDF_TARGET_LINUX
    ESP_RETURN_ON_FALSE(config->emul_conf != NULL, ESP_ERR_INVALID_ARG, TAG, "Linux mmap emulation configuration pointer can not be NULL");
#else
    ESP_RETURN_ON_FALSE(config->device_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Spi device pointer can not be NULL");
#endif //CONFIG_IDF_TARGET_LINUX

    if (!config->gc_factor) {
        config->gc_factor = 45;
    }

    *handle = calloc(1, sizeof(spi_nand_flash_device_t));
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

    esp_err_t ret = ESP_OK;

#ifdef CONFIG_IDF_TARGET_LINUX
    ESP_GOTO_ON_ERROR(detect_chip(*handle, config), fail, TAG, "Failed to detect nand chip");
#else
    ESP_GOTO_ON_ERROR(detect_chip(*handle), fail, TAG, "Failed to detect nand chip");
#endif
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

    ESP_GOTO_ON_ERROR(nand_register_dev(*handle), fail, TAG, "Failed to register nand dev");

    if ((*handle)->ops->init == NULL) {
        ESP_LOGE(TAG, "Failed to initialize spi_nand_ops");
        ret = ESP_FAIL;
        goto fail;
    }
    (*handle)->ops->init(*handle);

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

esp_err_t spi_nand_erase_chip(spi_nand_flash_device_t *handle)
{
    ESP_LOGW(TAG, "Entire chip is being erased");
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    ret = handle->ops->erase_chip(handle);
    if (ret) {
        goto end;
    }
    handle->ops->deinit(handle);

end:
    xSemaphoreGive(handle->mutex);
    return ret;
}

static bool s_need_data_refresh(spi_nand_flash_device_t *handle)
{
    uint8_t min_bits_corrected = 0;
    bool ret = false;
    if (handle->chip.ecc_data.ecc_corrected_bits_status == STAT_ECC_1_TO_3_BITS_CORRECTED) {
        min_bits_corrected = 1;
    } else if (handle->chip.ecc_data.ecc_corrected_bits_status == STAT_ECC_4_TO_6_BITS_CORRECTED) {
        min_bits_corrected = 4;
    } else if (handle->chip.ecc_data.ecc_corrected_bits_status == STAT_ECC_7_8_BITS_CORRECTED) {
        min_bits_corrected = 7;
    }

    // if number of corrected bits is greater than refresh threshold then rewrite the sector
    if (min_bits_corrected >= handle->chip.ecc_data.ecc_data_refresh_threshold) {
        ret = true;
    }
    return ret;
}

esp_err_t spi_nand_flash_read_sector(spi_nand_flash_device_t *handle, uint8_t *buffer, uint32_t sector_id)
{
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    ret = handle->ops->read(handle, buffer, sector_id);
    // After a successful read operation, check the ECC corrected bit status; if the read fails, return an error
    if (ret == ESP_OK && handle->chip.ecc_data.ecc_corrected_bits_status) {
        // This indicates a soft ECC error, we rewrite the sector to recover if corrected bits are greater than refresh threshold
        if (s_need_data_refresh(handle)) {
            ret = handle->ops->write(handle, buffer, sector_id);
        }
    }
    xSemaphoreGive(handle->mutex);

    return ret;
}

esp_err_t spi_nand_flash_copy_sector(spi_nand_flash_device_t *handle, uint32_t src_sec, uint32_t dst_sec)
{
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    ret = handle->ops->copy_sector(handle, src_sec, dst_sec);
    xSemaphoreGive(handle->mutex);

    return ret;
}

esp_err_t spi_nand_flash_write_sector(spi_nand_flash_device_t *handle, const uint8_t *buffer, uint32_t sector_id)
{
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    ret = handle->ops->write(handle, buffer, sector_id);
    xSemaphoreGive(handle->mutex);

    return ret;
}

esp_err_t spi_nand_flash_trim(spi_nand_flash_device_t *handle, uint32_t sector_id)
{
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    ret = handle->ops->trim(handle, sector_id);
    xSemaphoreGive(handle->mutex);

    return ret;
}

esp_err_t spi_nand_flash_sync(spi_nand_flash_device_t *handle)
{
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    ret = handle->ops->sync(handle);
    xSemaphoreGive(handle->mutex);

    return ret;
}

esp_err_t spi_nand_flash_get_capacity(spi_nand_flash_device_t *handle, uint32_t *number_of_sectors)
{
    return handle->ops->get_capacity(handle, number_of_sectors);
}

esp_err_t spi_nand_flash_get_sector_size(spi_nand_flash_device_t *handle, uint32_t *sector_size)
{
    *sector_size = handle->chip.page_size;
    return ESP_OK;
}

esp_err_t spi_nand_flash_get_block_size(spi_nand_flash_device_t *handle, uint32_t *block_size)
{
    *block_size = handle->chip.block_size;
    return ESP_OK;
}

esp_err_t spi_nand_flash_get_block_num(spi_nand_flash_device_t *handle, uint32_t *num_blocks)
{
    *num_blocks = handle->chip.num_blocks;
    return ESP_OK;
}

esp_err_t spi_nand_flash_deinit_device(spi_nand_flash_device_t *handle)
{
    esp_err_t ret = ESP_OK;
#ifdef CONFIG_IDF_TARGET_LINUX
    ret = nand_emul_deinit(handle);
#endif
    nand_unregister_dev(handle);
    free(handle->work_buffer);
    free(handle->read_buffer);
    free(handle->temp_buffer);
    vSemaphoreDelete(handle->mutex);
    free(handle);
    return ret;
}

/**************************************************************************************
 **************************************************************************************
 * Block Device Layer interface implementation
 **************************************************************************************
 */

static esp_err_t spi_nand_blockdev_read(esp_blockdev_handle_t handle, uint8_t *dst_buf, size_t dst_buf_size, uint64_t src_addr, size_t data_read_len)
{
    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)handle->ctx;

    esp_err_t res = spi_nand_flash_read_sector(dev_handle, dst_buf, src_addr);

    return res;
}

static esp_err_t spi_nand_blockdev_write(esp_blockdev_handle_t handle, const uint8_t *src_buf, uint64_t dst_addr, size_t data_write_len)
{
    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)handle->ctx;

    esp_err_t res = spi_nand_flash_write_sector(dev_handle, src_buf, dst_addr);

    return res;
}

static esp_err_t spi_nand_blockdev_erase(esp_blockdev_handle_t handle, uint64_t start_addr, size_t erase_len)
{
    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)handle->ctx;

    esp_err_t res = ESP_OK;
    uint32_t block_size = dev_handle->chip.block_size;;
    uint32_t start_block, block_count;
    if (start_addr % block_size != 0 || erase_len % block_size != 0 ) {
        return ESP_ERR_INVALID_ARG;
    }
    start_block = start_addr / block_size;
    block_count = erase_len / block_size;

    // Erase each block in the range
    for (uint32_t block = start_block; block < (block_count + start_block); block++) {
        res = dev_handle->ops->erase_block(dev_handle, block);
        if (res != ESP_OK) {
            break;
        }
    }

    return res;
}

static esp_err_t spi_nand_blockdev_sync(esp_blockdev_handle_t handle)
{
    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)handle->ctx;
    return spi_nand_flash_sync(dev_handle);
}

static esp_err_t spi_nand_blockdev_ioctl(esp_blockdev_handle_t handle, const uint8_t cmd, void *args)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)handle->ctx;

    switch (cmd) {
    case ESP_BLOCKDEV_CMD_GET_AVAILABLE_SECTORS: {
        uint32_t *sectors_available = (uint32_t *)args;
        spi_nand_flash_get_capacity(dev_handle, sectors_available);
    }
    break;

    default:
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

static esp_err_t spi_nand_release_blockdev(const esp_blockdev_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    spi_nand_flash_device_t *dev_handle = (spi_nand_flash_device_t *)handle->ctx;

    esp_err_t res = spi_nand_flash_deinit_device(dev_handle);
    if (res == ESP_OK) {
        free(handle);
    }

    return res;
}

static const esp_blockdev_ops_t spi_nand_blockdev_ops = {
    .read = spi_nand_blockdev_read,
    .write = spi_nand_blockdev_write,
    .erase = spi_nand_blockdev_erase,
    .ioctl = spi_nand_blockdev_ioctl,
    .sync = spi_nand_blockdev_sync,
    .release = spi_nand_release_blockdev,
};

esp_err_t spi_nand_flash_get_blockdev(spi_nand_flash_config_t *conf, spi_nand_flash_device_t **dev, esp_blockdev_handle_t *out_bdl_handle_ptr)
{
    if (conf == NULL || dev == NULL || out_bdl_handle_ptr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = spi_nand_flash_init_device(conf, dev);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_blockdev_t *blockdev = (esp_blockdev_t *) calloc(1, sizeof(esp_blockdev_t));
    if (blockdev == NULL) {
        return ESP_ERR_NO_MEM;
    }
    blockdev->ctx = *dev;

    ESP_BLOCKDEV_FLAGS_INST_CONFIG_DEFAULT(blockdev->device_flags);
    blockdev->ops = &spi_nand_blockdev_ops;

    // Set up geometry information
    uint32_t sector_size = (*dev)->chip.page_size;
    uint32_t block_size = (*dev)->chip.block_size;
    uint32_t num_blocks = (*dev)->chip.num_blocks;

    blockdev->geometry.disk_size = num_blocks * block_size;
    blockdev->geometry.write_size = sector_size;
    blockdev->geometry.read_size = sector_size;
    blockdev->geometry.erase_size = block_size;
    blockdev->geometry.recommended_write_size = sector_size;
    blockdev->geometry.recommended_read_size = sector_size;
    blockdev->geometry.recommended_erase_size = block_size;

    *out_bdl_handle_ptr = blockdev;
    return ESP_OK;
}
