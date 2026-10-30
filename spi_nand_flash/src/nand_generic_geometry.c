/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include <inttypes.h>
#include <string.h>
#include "esp_log.h"
#include "nand.h"
#include "nand_generic_geometry.h"

static const char *TAG = "nand_generic_geom";

static bool is_power_of_two(uint32_t n)
{
    return n != 0 && (n & (n - 1)) == 0;
}

esp_err_t nand_generic_geometry_try_init(spi_nand_flash_device_t *dev)
{
    const uint32_t page_size = CONFIG_NAND_FLASH_GENERIC_GEOMETRY_PAGE_SIZE;
    const uint32_t pages_per_block = CONFIG_NAND_FLASH_GENERIC_GEOMETRY_PAGES_PER_BLOCK;
    const uint32_t num_blocks = CONFIG_NAND_FLASH_GENERIC_GEOMETRY_NUM_BLOCKS;

    if (page_size == 0 || pages_per_block == 0 || num_blocks == 0) {
        ESP_LOGE(TAG, "ONFI parameter page failed and GENERIC_GEOMETRY_* unset (need page_size, pages_per_block, num_blocks)");
        return ESP_ERR_NOT_FOUND;
    }
    if (page_size < 512 || page_size > 8192 || !is_power_of_two(page_size)) {
        ESP_LOGE(TAG, "invalid GENERIC_GEOMETRY_PAGE_SIZE=%" PRIu32, page_size);
        return ESP_ERR_INVALID_ARG;
    }
    if (pages_per_block < 32 || pages_per_block > 256 || !is_power_of_two(pages_per_block)) {
        ESP_LOGE(TAG, "invalid GENERIC_GEOMETRY_PAGES_PER_BLOCK=%" PRIu32, pages_per_block);
        return ESP_ERR_INVALID_ARG;
    }

    dev->chip.log2_page_size = nand_log2_u32(page_size);
    dev->chip.log2_ppb = nand_log2_u32(pages_per_block);
    dev->chip.num_blocks = num_blocks;
    dev->chip.read_page_delay_us = CONFIG_NAND_FLASH_GENERIC_T_R_US;
    dev->chip.program_page_delay_us = CONFIG_NAND_FLASH_GENERIC_T_PROG_US;
    dev->chip.erase_block_delay_us = CONFIG_NAND_FLASH_GENERIC_T_BERS_US;
    /* Plane/QE/ECC policy applied by nand_generic_apply_profile(). */

    dev->chip_source = SPI_NAND_CHIP_SOURCE_KCONFIG;

    strlcpy(dev->device_info.chip_name, "generic-kconfig", sizeof(dev->device_info.chip_name));

    ESP_LOGW(TAG, "Geometry from Kconfig fallback (parameter page invalid/unavailable); verify against datasheet");
    return ESP_OK;
}
