/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "nand.h"
#include "nand_onfi.h"
#include "nand_onfi_param_page.h"
#include "nand_onfi_crc.h"
#include "spi_nand_oper.h"

static const char *TAG = "nand_onfi";

static bool is_power_of_two(uint32_t n)
{
    return n != 0 && (n & (n - 1)) == 0;
}

static esp_err_t parse_parameter_page(spi_nand_flash_device_t *dev, const nand_parameter_page_t *param)
{
    if (memcmp(param->signature, "ONFI", NAND_ONFI_PARAM_PAGE_SIGNATURE_LEN) != 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if (!nand_onfi_param_page_crc_valid((const uint8_t *)param, NAND_ONFI_PARAM_PAGE_SIZE)) {
        return ESP_ERR_INVALID_CRC;
    }
    if (param->num_luns != 1) {
        ESP_LOGW(TAG, "ONFI parameter page: num_luns=%u (only single-LUN supported)", param->num_luns);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (param->data_bytes_per_page < 512 || param->data_bytes_per_page > 8192
            || !is_power_of_two(param->data_bytes_per_page)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (param->pages_per_block < 32 || param->pages_per_block > 256
            || !is_power_of_two(param->pages_per_block)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (param->blocks_per_lun == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    dev->chip.log2_page_size = nand_log2_u32(param->data_bytes_per_page);
    dev->chip.log2_ppb = nand_log2_u32(param->pages_per_block);
    dev->chip.num_blocks = param->blocks_per_lun;
    /* A CRC-valid page can still leave a timing at 0. Geometry stays; the
     * zero field uses the same Kconfig bound as the geometry-fallback path.
     * nand_wait_for_ready() treats this as the expected busy time (timeout is
     * 10x). A non-zero ONFI value is kept. */
    if (param->t_r_max_us == 0) {
        ESP_LOGW(TAG, "ONFI tR is 0; using CONFIG_NAND_FLASH_GENERIC_T_R_US=%d us",
                 CONFIG_NAND_FLASH_GENERIC_T_R_US);
        dev->chip.read_page_delay_us = CONFIG_NAND_FLASH_GENERIC_T_R_US;
    } else {
        dev->chip.read_page_delay_us = param->t_r_max_us;
    }
    if (param->t_prog_max_us == 0) {
        ESP_LOGW(TAG, "ONFI tPROG is 0; using CONFIG_NAND_FLASH_GENERIC_T_PROG_US=%d us",
                 CONFIG_NAND_FLASH_GENERIC_T_PROG_US);
        dev->chip.program_page_delay_us = CONFIG_NAND_FLASH_GENERIC_T_PROG_US;
    } else {
        dev->chip.program_page_delay_us = param->t_prog_max_us;
    }
    if (param->t_bers_max_us == 0) {
        ESP_LOGW(TAG, "ONFI tBERS is 0; using CONFIG_NAND_FLASH_GENERIC_T_BERS_US=%d us",
                 CONFIG_NAND_FLASH_GENERIC_T_BERS_US);
        dev->chip.erase_block_delay_us = CONFIG_NAND_FLASH_GENERIC_T_BERS_US;
    } else {
        dev->chip.erase_block_delay_us = param->t_bers_max_us;
    }
    /* Bytes 113–114 (ONFI interleaved addressing) are not mapped to this
     * driver's plane-select column bit. Placeholder until
     * nand_generic_apply_profile() applies CONFIG_NAND_FLASH_GENERIC_NUM_PLANES. */
    dev->chip.num_planes = 1;
    dev->chip.has_quad_enable_bit = 0;
    dev->chip.quad_enable_bit_pos = 0;

    /* Byte 64 is not the Read ID (9Fh) manufacturer byte. nand_init_device()
     * already stored that. A blank or stale byte 64 must not replace it. */
    if (param->jedec_id != 0 && param->jedec_id != dev->device_info.manufacturer_id) {
        ESP_LOGW(TAG,
                 "ONFI byte 64 (0x%02x) differs from Read ID manufacturer 0x%02x; keeping Read ID",
                 param->jedec_id, dev->device_info.manufacturer_id);
    }
    snprintf(dev->device_info.chip_name, sizeof(dev->device_info.chip_name),
             "%.12s %.20s", param->manufacturer, param->model);

    ESP_LOGD(TAG, "ONFI spare_bytes_per_page=%u (not used for geometry; markers need >= 4 OOB bytes)",
             (unsigned)param->spare_bytes_per_page);
    if (param->spare_bytes_per_page > 0 && param->spare_bytes_per_page < 4) {
        ESP_LOGW(TAG,
                 "ONFI spare_bytes_per_page=%u < 4; BBM/page-used markers occupy 4 bytes at column page_size",
                 (unsigned)param->spare_bytes_per_page);
    }

    return ESP_OK;
}

esp_err_t nand_onfi_try_init(spi_nand_flash_device_t *dev)
{
    esp_err_t ret;
    const size_t alignment = spi_nand_get_dma_alignment();
    /* 256 is a multiple of the DMA alignment, so half-duplex reads this buffer directly.
     * Full-duplex pads the length and bounces through temp_buffer. */
    uint8_t *raw_buf = heap_caps_aligned_alloc(alignment, NAND_ONFI_PARAM_PAGE_SIZE,
                                               MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(raw_buf != NULL, ESP_ERR_NO_MEM, TAG, "parameter page buffer alloc failed");

    /* Full-duplex only: param-page bounce in temp_buffer. Allocate/free only if unset. */
    uint8_t *bounce = NULL;
    bool bounce_owned = false;
    if (!(dev->config.flags & SPI_DEVICE_HALFDUPLEX) && dev->temp_buffer == NULL) {
        bounce = heap_caps_aligned_alloc(alignment, NAND_ONFI_PARAM_PAGE_SIZE + alignment,
                                         MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        if (bounce == NULL) {
            free(raw_buf);
            ESP_LOGE(TAG, "ONFI full-duplex bounce alloc failed");
            return ESP_ERR_NO_MEM;
        }
        dev->temp_buffer = bounce;
        bounce_owned = true;
    }

    ret = nand_onfi_read_parameter_page(dev, raw_buf, NAND_ONFI_PARAM_PAGE_SIZE);
    if (bounce_owned) {
        free(bounce);
        dev->temp_buffer = NULL;
    }
    if (ret != ESP_OK) {
        free(raw_buf);
        return ret;
    }

    ret = parse_parameter_page(dev, (const nand_parameter_page_t *)raw_buf);
    free(raw_buf);
    if (ret != ESP_OK) {
        return ret;
    }

    dev->chip_source = SPI_NAND_CHIP_SOURCE_ONFI;

    ESP_LOGW(TAG,
             "Chip detected via ONFI parameter page: page_size=%u, pages_per_block=%u, blocks=%" PRIu32
             ", t_r=%" PRIu32 " us, t_prog=%" PRIu32 " us, t_bers=%" PRIu32
             " us; vendor DB bypassed — verify before production use",
             (unsigned)(1U << dev->chip.log2_page_size),
             (unsigned)(1U << dev->chip.log2_ppb),
             dev->chip.num_blocks,
             (uint32_t)dev->chip.read_page_delay_us,
             (uint32_t)dev->chip.program_page_delay_us,
             (uint32_t)dev->chip.erase_block_delay_us);

    return ESP_OK;
}
