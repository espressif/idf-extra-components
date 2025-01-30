/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2024 Espressif Systems (Shanghai) CO LTD
 */

#pragma once

#include <stdint.h>
#include "spi_nand_flash.h"
#ifdef CONFIG_IDF_TARGET_LINUX
#include "freertos/FreeRTOS.h"
#include "nand_linux_mmap_emul.h"
#endif
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INVALID_PAGE 0xFFFF

typedef enum {
    STAT_ECC_OK = 0,
    STAT_ECC_1_TO_3_BITS_CORRECTED = 1,
    STAT_ECC_BITS_CORRECTED = STAT_ECC_1_TO_3_BITS_CORRECTED,
    STAT_ECC_NOT_CORRECTED = 2,
    STAT_ECC_4_TO_6_BITS_CORRECTED = 3,
    STAT_ECC_MAX_BITS_CORRECTED = STAT_ECC_4_TO_6_BITS_CORRECTED,
    STAT_ECC_7_8_BITS_CORRECTED = 5,
    STAT_ECC_MAX
} ecc_status_t;

typedef struct {
    uint8_t ecc_status_reg_len_in_bits;
    uint8_t ecc_data_refresh_threshold;
    ecc_status_t ecc_corrected_bits_status;
} ecc_data_t;

typedef struct {
    uint8_t log2_page_size; //is power of 2, log2_page_size shift (1<<log2_page_size) is stored to page_size
    uint8_t log2_ppb;  //is power of 2, log2_ppb shift ((1<<log2_ppb) * page_size) will be stored in block size
    uint32_t block_size;
    uint32_t page_size;
#ifdef CONFIG_IDF_TARGET_LINUX
    uint32_t emulated_page_size;
    uint32_t emulated_page_oob;
#endif
    uint32_t num_blocks;
    uint32_t read_page_delay_us;
    uint32_t erase_block_delay_us;
    uint32_t program_page_delay_us;
    ecc_data_t ecc_data;
} spi_nand_chip_t;

typedef struct {
    esp_err_t (*init)(spi_nand_flash_device_t *handle);
    esp_err_t (*deinit)(spi_nand_flash_device_t *handle);
    esp_err_t (*read)(spi_nand_flash_device_t *handle, uint8_t *buffer, uint32_t sector_id);
    esp_err_t (*write)(spi_nand_flash_device_t *handle, const uint8_t *buffer, uint32_t sector_id);
    esp_err_t (*erase_chip)(spi_nand_flash_device_t *handle);
    esp_err_t (*erase_block)(spi_nand_flash_device_t *handle, uint32_t block);
    esp_err_t (*trim)(spi_nand_flash_device_t *handle, uint32_t sector_id);
    esp_err_t (*sync)(spi_nand_flash_device_t *handle);
    esp_err_t (*copy_sector)(spi_nand_flash_device_t *handle, uint32_t src_sec, uint32_t dst_sec);
    esp_err_t (*get_capacity)(spi_nand_flash_device_t *handle, uint32_t *number_of_sectors);
} spi_nand_ops;

struct spi_nand_flash_device_t {
    spi_nand_flash_config_t config;
    spi_nand_chip_t chip;
    const spi_nand_ops *ops;
    void *ops_priv_data;
    uint8_t *work_buffer;
    uint8_t *read_buffer;
    SemaphoreHandle_t mutex;
#ifdef CONFIG_IDF_TARGET_LINUX
    nand_mmap_emul_handle_t *emul_handle;
#endif
};

esp_err_t nand_register_dev(spi_nand_flash_device_t *handle);
esp_err_t nand_unregister_dev(spi_nand_flash_device_t *handle);

#ifdef __cplusplus
}
#endif
