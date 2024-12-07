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
#include "spi_nand_oper.h"
#include "nand_impl.h"
#include "nand.h"
#include "nand_flash_devices.h"
#include "nand_flash_chip.h"
#include "esp_vfs_fat_nand.h"

static const char *TAG = "nand_flash";

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
    spi_nand_execute_transaction(dev->config.device_handle, &t);
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
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
}

static esp_err_t unprotect_chip(spi_nand_flash_device_t *dev)
{
    uint8_t status;
    esp_err_t ret = spi_nand_read_register(dev->config.device_handle, REG_PROTECT, &status);
    if (ret != ESP_OK) {
        return ret;
    }

    if (status != 0x00) {
        ret = spi_nand_write_register(dev->config.device_handle, REG_PROTECT, 0);
    }

    return ret;
}

esp_err_t spi_nand_flash_init_device(spi_nand_flash_config_t *config, spi_nand_flash_device_t **handle)
{
    ESP_RETURN_ON_FALSE(config->device_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Spi device pointer can not be NULL");

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

    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_ERROR(detect_chip(*handle), fail, TAG, "Failed to detect nand chip");
    ESP_GOTO_ON_ERROR(unprotect_chip(*handle), fail, TAG, "Failed to clear protection register");

    (*handle)->chip.page_size = 1 << (*handle)->chip.log2_page_size;
    (*handle)->chip.block_size = (1 << (*handle)->chip.log2_ppb) * (*handle)->chip.page_size;

    (*handle)->work_buffer = heap_caps_malloc((*handle)->chip.page_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE((*handle)->work_buffer != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");

    (*handle)->read_buffer = heap_caps_malloc((*handle)->chip.page_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE((*handle)->read_buffer != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");

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
    vSemaphoreDelete((*handle)->mutex);
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

    // if number of corrected bits is greater than refresh threshold then rewite the sector
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
    nand_unregister_dev(handle);
    free(handle->work_buffer);
    free(handle->read_buffer);
    vSemaphoreDelete(handle->mutex);
    free(handle);
    return ESP_OK;
}
