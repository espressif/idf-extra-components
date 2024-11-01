/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2023 Espressif Systems (Shanghai) CO LTD
 */

#include <string.h>
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "spi_nand_flash.h"
#include "dhara/error.h"
#include "spi_nand_oper.h"
#include "nand.h"
#include "nand_flash_devices.h"
#include "esp_vfs_fat_nand.h"

static const char *TAG = "nand_flash";

static esp_err_t spi_nand_winbond_init(spi_nand_flash_device_t *dev)
{
    uint8_t device_id_buf[2];
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .dummy_bits = 16,
        .miso_len = 2,
        .miso_data = device_id_buf,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    spi_nand_execute_transaction(dev->config.device_handle, &t);
    uint16_t device_id = (device_id_buf[0] << 8) + device_id_buf[1];
    dev->read_page_delay_us = 10;
    dev->erase_block_delay_us = 2500;
    dev->program_page_delay_us = 320;
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);
    switch (device_id) {
    case WINBOND_DI_AA20:
    case WINBOND_DI_BA20:
        dev->dhara_nand.num_blocks = 512;
        break;
    case WINBOND_DI_AA21:
    case WINBOND_DI_BA21:
    case WINBOND_DI_BC21:
        dev->dhara_nand.num_blocks = 1024;
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static esp_err_t spi_nand_alliance_init(spi_nand_flash_device_t *dev)
{
    uint8_t device_id;
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .address = 1,
        .address_bytes = 1,
        .dummy_bits = 8,
        .miso_len = 1,
        .miso_data = &device_id,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    spi_nand_execute_transaction(dev->config.device_handle, &t);
    dev->erase_block_delay_us = 3000;
    dev->program_page_delay_us = 630;
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);
    switch (device_id) {
    case ALLIANCE_DI_25: //AS5F31G04SND-08LIN
        dev->dhara_nand.num_blocks = 1024;
        dev->read_page_delay_us = 60;
        break;
    case ALLIANCE_DI_2E: //AS5F32G04SND-08LIN
    case ALLIANCE_DI_8E: //AS5F12G04SND-10LIN
        dev->dhara_nand.num_blocks = 2048;
        dev->read_page_delay_us = 60;
        break;
    case ALLIANCE_DI_2F: //AS5F34G04SND-08LIN
    case ALLIANCE_DI_8F: //AS5F14G04SND-10LIN
        dev->dhara_nand.num_blocks = 4096;
        dev->read_page_delay_us = 60;
        break;
    case ALLIANCE_DI_2D: //AS5F38G04SND-08LIN
    case ALLIANCE_DI_8D: //AS5F18G04SND-10LIN
        dev->dhara_nand.log2_page_size = 12; // 4k pages
        dev->dhara_nand.num_blocks = 4096;
        dev->read_page_delay_us = 130; // somewhat slower reads
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static esp_err_t spi_nand_gigadevice_init(spi_nand_flash_device_t *dev)
{
    uint8_t device_id;
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .dummy_bits = 16,
        .miso_len = 1,
        .miso_data = &device_id,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    spi_nand_execute_transaction(dev->config.device_handle, &t);
    dev->read_page_delay_us = 25;
    dev->erase_block_delay_us = 3200;
    dev->program_page_delay_us = 380;
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);
    switch (device_id) {
    case GIGADEVICE_DI_51:
    case GIGADEVICE_DI_41:
    case GIGADEVICE_DI_31:
    case GIGADEVICE_DI_21:
        dev->dhara_nand.num_blocks = 1024;
        break;
    case GIGADEVICE_DI_52:
    case GIGADEVICE_DI_42:
    case GIGADEVICE_DI_32:
    case GIGADEVICE_DI_22:
        dev->dhara_nand.num_blocks = 2048;
        break;
    case GIGADEVICE_DI_55:
    case GIGADEVICE_DI_45:
    case GIGADEVICE_DI_35:
    case GIGADEVICE_DI_25:
        dev->dhara_nand.num_blocks = 4096;
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static esp_err_t spi_nand_micron_init(spi_nand_flash_device_t *dev)
{
    uint8_t device_id;
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .dummy_bits = 16,
        .miso_len = 1,
        .miso_data = &device_id,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    spi_nand_execute_transaction(dev->config.device_handle, &t);
    dev->ecc_data.ecc_status_reg_len_in_bits = 3;
    dev->erase_block_delay_us = 2000;
    ESP_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);
    switch (device_id) {
    case MICRON_DI_34:
        dev->read_page_delay_us = 115;
        dev->program_page_delay_us = 240;
        dev->dhara_nand.num_blocks = 2048;
        dev->dhara_nand.log2_ppb = 6;        // 64 pages per block
        dev->dhara_nand.log2_page_size = 12; // 4096 bytes per page
        break;
    case MICRON_DI_14:
    case MICRON_DI_15:
        dev->read_page_delay_us = 46;
        dev->program_page_delay_us = 220;
        dev->dhara_nand.num_blocks = 1024;
        dev->dhara_nand.log2_ppb = 6;          // 64 pages per block
        dev->dhara_nand.log2_page_size = 11;   // 2048 bytes per page
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

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

    (*handle)->ecc_data.ecc_status_reg_len_in_bits = 2;
    (*handle)->ecc_data.ecc_data_refresh_threshold = 4;
    (*handle)->dhara_nand.log2_ppb = 6; // 64 pages per block is standard
    (*handle)->dhara_nand.log2_page_size = 11;  // 2048 bytes per page is fairly standard

    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_ERROR(detect_chip(*handle), fail, TAG, "Failed to detect nand chip");
    ESP_GOTO_ON_ERROR(unprotect_chip(*handle), fail, TAG, "Failed to clear protection register");

    (*handle)->page_size = 1 << (*handle)->dhara_nand.log2_page_size;
    (*handle)->block_size = (1 << (*handle)->dhara_nand.log2_ppb) * (*handle)->page_size;
    (*handle)->num_blocks = (*handle)->dhara_nand.num_blocks;

    (*handle)->work_buffer = heap_caps_malloc((*handle)->page_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE((*handle)->work_buffer != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");

    (*handle)->read_buffer = heap_caps_malloc((*handle)->page_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE((*handle)->read_buffer != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");

    (*handle)->mutex = xSemaphoreCreateMutex();
    if (!(*handle)->mutex) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }

    dhara_map_init(&(*handle)->dhara_map, &(*handle)->dhara_nand, (*handle)->work_buffer, config->gc_factor);

    dhara_error_t ignored;
    dhara_map_resume(&(*handle)->dhara_map, &ignored);

    return ret;

fail:
    if ((*handle)->work_buffer != NULL) {
        free((*handle)->work_buffer);
    }
    if ((*handle)->read_buffer != NULL) {
        free((*handle)->read_buffer);
    }
    if ((*handle)->mutex != NULL) {
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

    for (int i = 0; i < handle->num_blocks; i++) {
        ESP_GOTO_ON_ERROR(spi_nand_write_enable(handle->config.device_handle), end, TAG, "");
        ESP_GOTO_ON_ERROR(spi_nand_erase_block(handle->config.device_handle, i * (1 << handle->dhara_nand.log2_ppb)),
                          end, TAG, "");
        ESP_GOTO_ON_ERROR(wait_for_ready(handle->config.device_handle, handle->erase_block_delay_us, NULL),
                          end, TAG, "");
    }

    // clear dhara map
    dhara_map_init(&handle->dhara_map, &handle->dhara_nand, handle->work_buffer, handle->config.gc_factor);
    dhara_map_clear(&handle->dhara_map);

end:
    xSemaphoreGive(handle->mutex);
    return ret;
}

static bool s_need_data_refresh(spi_nand_flash_device_t *handle)
{
    uint8_t min_bits_corrected = 0;
    bool ret = false;
    if (handle->ecc_data.ecc_corrected_bits_status == STAT_ECC_1_TO_3_BITS_CORRECTED) {
        min_bits_corrected = 1;
    } else if (handle->ecc_data.ecc_corrected_bits_status == STAT_ECC_4_TO_6_BITS_CORRECTED) {
        min_bits_corrected = 4;
    } else if (handle->ecc_data.ecc_corrected_bits_status == STAT_ECC_7_8_BITS_CORRECTED) {
        min_bits_corrected = 7;
    }

    // if number of corrected bits is greater than refresh threshold then rewite the sector
    if (min_bits_corrected >= handle->ecc_data.ecc_data_refresh_threshold) {
        ret = true;
    }
    return ret;
}

esp_err_t spi_nand_flash_read_sector(spi_nand_flash_device_t *handle, uint8_t *buffer, dhara_sector_t sector_id)
{
    dhara_error_t err;
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    // After a successful read operation, check the ECC corrected bit status; if the read fails, return an error
    if (dhara_map_read(&handle->dhara_map, sector_id, handle->read_buffer, &err)) {
        ret = ESP_ERR_FLASH_BASE + err;
    } else if (handle->ecc_data.ecc_corrected_bits_status) {
        // This indicates a soft ECC error, we rewrite the sector to recover if corrected bits are greater than refresh threshold
        if (s_need_data_refresh(handle)) {
            if (dhara_map_write(&handle->dhara_map, sector_id, handle->read_buffer, &err)) {
                ret = ESP_ERR_FLASH_BASE + err;
            }
        }
    }

    xSemaphoreGive(handle->mutex);
    memcpy(buffer, handle->read_buffer, handle->page_size);
    return ret;
}

esp_err_t spi_nand_flash_copy_sector(spi_nand_flash_device_t *handle, dhara_sector_t src_sec, dhara_sector_t dst_sec)
{
    dhara_error_t err;
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    if (dhara_map_copy_sector(&handle->dhara_map, src_sec, dst_sec, &err)) {
        ret = ESP_ERR_FLASH_BASE + err;
    }

    xSemaphoreGive(handle->mutex);
    return ret;
}

esp_err_t spi_nand_flash_write_sector(spi_nand_flash_device_t *handle, const uint8_t *buffer, dhara_sector_t sector_id)
{
    dhara_error_t err;
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    if (dhara_map_write(&handle->dhara_map, sector_id, buffer, &err)) {
        ret = ESP_ERR_FLASH_BASE + err;
    }

    xSemaphoreGive(handle->mutex);
    return ret;
}

esp_err_t spi_nand_flash_trim(spi_nand_flash_device_t *handle, dhara_sector_t sector_id)
{
    dhara_error_t err;
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    if (dhara_map_trim(&handle->dhara_map, sector_id, &err)) {
        ret = ESP_ERR_FLASH_BASE + err;
    }

    xSemaphoreGive(handle->mutex);
    return ret;
}

esp_err_t spi_nand_flash_sync(spi_nand_flash_device_t *handle)
{
    dhara_error_t err;
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    if (dhara_map_sync(&handle->dhara_map, &err)) {
        return ESP_ERR_FLASH_BASE + err;
    }

    xSemaphoreGive(handle->mutex);
    return ret;
}

esp_err_t spi_nand_flash_get_capacity(spi_nand_flash_device_t *handle, dhara_sector_t *number_of_sectors)
{
    *number_of_sectors = dhara_map_capacity(&handle->dhara_map);
    return ESP_OK;
}

esp_err_t spi_nand_flash_get_sector_size(spi_nand_flash_device_t *handle, uint32_t *sector_size)
{
    *sector_size = handle->page_size;
    return ESP_OK;
}

esp_err_t spi_nand_flash_deinit_device(spi_nand_flash_device_t *handle)
{
    free(handle->work_buffer);
    free(handle->read_buffer);
    vSemaphoreDelete(handle->mutex);
    free(handle);
    return ESP_OK;
}
