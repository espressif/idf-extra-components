/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2026 Espressif Systems (Shanghai) CO LTD
 */

#include "nand_oob_device.h"

#include <assert.h>
#include <string.h>

#include "esp_check.h"
#include "nand.h"
#include "nand_oob_layout_default.h"

static const char *TAG = "nand_oob_dev";

static esp_err_t nand_oob_spare_bytes_for_handle(const spi_nand_flash_device_t *handle, uint16_t *spare_out)
{
#ifdef CONFIG_IDF_TARGET_LINUX
    *spare_out = (uint16_t)handle->chip.emulated_page_oob;
    return ESP_OK;
#else
    switch (handle->chip.page_size) {
    case 512:
        *spare_out = 16;
        return ESP_OK;
    case 2048:
        *spare_out = 64;
        return ESP_OK;
    case 4096:
        *spare_out = 128;
        return ESP_OK;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif
}

esp_err_t nand_oob_device_layout_init(spi_nand_flash_device_t *handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");

    uint16_t spare = 0;
    ESP_RETURN_ON_ERROR(nand_oob_spare_bytes_for_handle(handle, &spare), TAG, "spare size");
    ESP_RETURN_ON_FALSE(spare >= 4, ESP_ERR_INVALID_SIZE, TAG, "spare too small for marker layout");

    memset(handle->oob_fields, 0, sizeof(handle->oob_fields));
    memset(handle->oob_cached_regs_free_ecc, 0, sizeof(handle->oob_cached_regs_free_ecc));
    memset(handle->oob_cached_regs_free_no_ecc, 0, sizeof(handle->oob_cached_regs_free_no_ecc));
    handle->oob_cached_reg_count_free_ecc = 0;
    handle->oob_cached_reg_count_free_no_ecc = 0;

    handle->oob_layout = nand_oob_layout_get_default();

    spi_nand_oob_field_spec_t *pu = &handle->oob_fields[SPI_NAND_OOB_FIELD_PAGE_USED];
    pu->id = SPI_NAND_OOB_FIELD_PAGE_USED;
    pu->length = 2;
    pu->oob_class = SPI_NAND_OOB_CLASS_FREE_ECC;
    pu->logical_offset = 0;
    pu->assigned = true;

    for (int section = 0;; section++) {
        spi_nand_oob_region_desc_t desc;
        esp_err_t err = handle->oob_layout->ops->free_region(handle, section, &desc);
        if (err == ESP_ERR_NOT_FOUND) {
            break;
        }
        ESP_RETURN_ON_ERROR(err, TAG, "free_region");
        if (!desc.programmable) {
            continue;
        }
        ESP_RETURN_ON_FALSE((uint32_t)desc.offset + (uint32_t)desc.length <= (uint32_t)spare,
                            ESP_ERR_INVALID_SIZE, TAG, "free region out of spare");
        if (desc.ecc_protected) {
            ESP_RETURN_ON_FALSE(handle->oob_cached_reg_count_free_ecc < SPI_NAND_OOB_MAX_REGIONS,
                                ESP_ERR_NO_MEM, TAG, "too many FREE_ECC OOB regions");
            handle->oob_cached_regs_free_ecc[handle->oob_cached_reg_count_free_ecc++] = desc;
        } else {
            ESP_RETURN_ON_FALSE(handle->oob_cached_reg_count_free_no_ecc < SPI_NAND_OOB_MAX_REGIONS,
                                ESP_ERR_NO_MEM, TAG, "too many FREE_NOECC OOB regions");
            handle->oob_cached_regs_free_no_ecc[handle->oob_cached_reg_count_free_no_ecc++] = desc;
        }
    }

#ifndef NDEBUG
    if (handle->oob_layout == nand_oob_layout_get_default()) {
        size_t total_ecc = 0;
        for (unsigned i = 0; i < handle->oob_cached_reg_count_free_ecc; i++) {
            total_ecc += handle->oob_cached_regs_free_ecc[i].length;
        }
        assert(total_ecc == 2);
        assert(handle->oob_cached_reg_count_free_ecc == 1);
        assert(handle->oob_cached_regs_free_ecc[0].offset == 2 && handle->oob_cached_regs_free_ecc[0].length == 2);
        assert(handle->oob_cached_reg_count_free_no_ecc == 0);
    }
#endif

    return ESP_OK;
}
