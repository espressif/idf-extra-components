/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2024 Espressif Systems (Shanghai) CO LTD
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#ifndef CONFIG_IDF_TARGET_LINUX
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct spi_nand_flash_device_t spi_nand_flash_device_t;


#ifdef CONFIG_IDF_TARGET_LINUX
#include "nand_linux_mmap_emul.h"
#endif

/** @brief SPI mode used for reading from SPI NAND Flash */
typedef enum {
    SPI_NAND_IO_MODE_SIO = 0,
    SPI_NAND_IO_MODE_DOUT,
    SPI_NAND_IO_MODE_DIO,
    SPI_NAND_IO_MODE_QOUT,
    SPI_NAND_IO_MODE_QIO,
} spi_nand_flash_io_mode_t;

/** @brief Structure to describe how to configure the nand access layer.
 @note For DIO and DOUT mode The spi_device_handle_t must be initialized with the flag SPI_DEVICE_HALFDUPLEX
 SIO mode can be initialized with half-duplex or full-duplex mode
*/
struct spi_nand_flash_config_t {
#ifndef CONFIG_IDF_TARGET_LINUX
    spi_device_handle_t device_handle;       ///< SPI Device for this nand chip.
#else
    nand_file_mmap_emul_config_t *emul_conf;
#endif
    uint8_t gc_factor;                       ///< The gc factor controls the number of blocks to spare block ratio.
    ///< Lower values will reduce the available space but increase performance
    spi_nand_flash_io_mode_t io_mode;        ///< set io mode for SPI NAND communication
    uint8_t flags;                           ///< set flag with SPI_DEVICE_HALFDUPLEX for half duplex communcation, 0 for full-duplex.
    ///< This flag value must match the flag value in the spi_device_interface_config_t structure.
};

typedef struct spi_nand_flash_config_t spi_nand_flash_config_t;

/** @brief Initialise SPI nand flash chip interface.
 *
 * This function must be called before calling any other API functions for the nand flash.
 *
 * @param config Pointer to SPI nand flash config structure.
 * @param[out] handle The handle to the SPI nand flash chip is returned in this variable.
 * @return ESP_OK on success, or a flash error code if the initialisation failed.
 */
esp_err_t spi_nand_flash_init_device(spi_nand_flash_config_t *config, spi_nand_flash_device_t **handle);

/** @brief Read a sector from the nand flash.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] buffer The output buffer to put the read data into.
 * @param sector_id The id of the sector to read.
 * @return ESP_OK on success, or a flash error code if the read failed.
 */
esp_err_t spi_nand_flash_read_sector(spi_nand_flash_device_t *handle, uint8_t *buffer, uint32_t sector_id);

/** @brief Copy a sector to another sector from the nand flash.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param src_sec The source sector id from which data to be copied.
 * @param dst_sec The destination sector id to which data should be copied.
 * @return ESP_OK on success, or a flash error code if the copy failed.
 */
esp_err_t spi_nand_flash_copy_sector(spi_nand_flash_device_t *handle, uint32_t src_sec, uint32_t dst_sec);

/** @brief Write a sector to the nand flash.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] buffer The input buffer containing the data to write.
 * @param sector_id The id of the sector to write.
 * @return ESP_OK on success, or a flash error code if the write failed.
 */
esp_err_t spi_nand_flash_write_sector(spi_nand_flash_device_t *handle, const uint8_t *buffer, uint32_t sector_id);

/** @brief Trim sector from the nand flash.
 *
 * This function marks specified sector as free to optimize memory usage
 * and support wear-leveling. Typically invoked when files are deleted or
 * resized to allow the underlying storage to manage these sectors.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param sector_id The id of the sector to be trimmed.
 * @return ESP_OK on success, or a flash error code if the trim failed.
 */
esp_err_t spi_nand_flash_trim(spi_nand_flash_device_t *handle, uint32_t sector_id);

/** @brief Synchronizes any cache to the device.
 *
 * After this method is called, the nand flash chip should be synchronized with the results of any previous read/writes.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @return ESP_OK on success, or a flash error code if the synchronization failed.
 */
esp_err_t spi_nand_flash_sync(spi_nand_flash_device_t *handle);

/** @brief Retrieve the number of sectors available.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] number_of_sectors A pointer of where to put the return value
 * @return ESP_OK on success, or a flash error code if the operation failed.
 */
esp_err_t spi_nand_flash_get_capacity(spi_nand_flash_device_t *handle, uint32_t *number_of_sectors);

/** @brief Retrieve the size of each sector.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] sectors_size A pointer of where to put the return value
 * @return ESP_OK on success, or a flash error code if the operation failed.
 */
esp_err_t spi_nand_flash_get_sector_size(spi_nand_flash_device_t *handle, uint32_t *sector_size);

/** @brief Retrieve the size of each block.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] block_size A pointer of where to put the return value
 * @return ESP_OK on success, or a flash error code if the operation failed.
 */
esp_err_t spi_nand_flash_get_block_size(spi_nand_flash_device_t *handle, uint32_t *block_size);

/** @brief Erases the entire chip, invalidating any data on the chip.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @return ESP_OK on success, or a flash error code if the erase failed.
 */
esp_err_t spi_nand_erase_chip(spi_nand_flash_device_t *handle);

/** @brief Retrieve the number of blocks available.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] number_of_blocks A pointer of where to put the return value
 * @return ESP_OK on success, or a flash error code if the operation failed.
 */
esp_err_t spi_nand_flash_get_block_num(spi_nand_flash_device_t *handle, uint32_t *number_of_blocks);

/** @brief De-initialize the handle, releasing any resources reserved.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @return ESP_OK on success, or a flash error code if the de-initialization failed.
 */
esp_err_t spi_nand_flash_deinit_device(spi_nand_flash_device_t *handle);

#ifdef __cplusplus
}
#endif
