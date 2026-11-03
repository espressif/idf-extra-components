/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2025 Espressif Systems (Shanghai) CO LTD
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "nand_device_types.h"
#ifndef CONFIG_IDF_TARGET_LINUX
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#endif

#ifdef CONFIG_NAND_FLASH_ENABLE_BDL
#include "esp_blockdev.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct spi_nand_flash_device_t spi_nand_flash_device_t;

/** @brief How the attached NAND chip was identified at init */
typedef enum {
    SPI_NAND_CHIP_SOURCE_DATABASE = 0,  /*!< Vendor device table */
    SPI_NAND_CHIP_SOURCE_ONFI,          /*!< Geometry parsed from the ONFI parameter page */
    SPI_NAND_CHIP_SOURCE_KCONFIG,       /*!< Geometry supplied via Kconfig fallback */
} spi_nand_chip_source_t;

/** True when init used the generic detection path (ONFI or Kconfig), not the vendor DB. */
static inline bool spi_nand_chip_source_is_generic(spi_nand_chip_source_t source)
{
    return source == SPI_NAND_CHIP_SOURCE_ONFI || source == SPI_NAND_CHIP_SOURCE_KCONFIG;
}

/**
 * @brief Probe / bring-up report for the generic chip detection path.
 *
 * Always declared so apps can call ESP_BLOCKDEV_CMD_GET_GENERIC_PROBE_REPORT
 * without a Kconfig compile guard. Filled when
 * CONFIG_NAND_FLASH_GENERIC_CHIP_DETECTION is enabled (that path always uses
 * ONFI or Kconfig); otherwise the ioctl returns ESP_ERR_NOT_SUPPORTED.
 * param_page_valid distinguishes ONFI vs Kconfig geometry origin.
 */
typedef struct {
    nand_device_info_t device_info;      /*!< JEDEC IDs and chip name from init */
    bool param_page_valid;              /*!< True if ONFI parameter page signature+CRC passed */
    bool page0_read_ok;                 /*!< Non-destructive page 0 read probe */
    bool write_erase_enabled;           /*!< CONFIG_NAND_FLASH_GENERIC_WRITE_ERASE_ENABLE */
    bool on_die_ecc_enabled;            /*!< CFG bit 4 after init: read-only path reports power-up;
                                         *   WRITE_ERASE path sets the bit then re-reads */
    uint32_t page_size;
    uint32_t pages_per_block;
    uint32_t num_blocks;
    uint8_t num_planes;                 /*!< Plane count (CONFIG_NAND_FLASH_GENERIC_NUM_PLANES) */
} spi_nand_generic_probe_report_t;

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
    uint8_t flags;                           ///< set flag with SPI_DEVICE_HALFDUPLEX for half duplex communication, 0 for full-duplex.
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
 *
 * @note When CONFIG_NAND_FLASH_ENABLE_BDL is enabled, this function returns ESP_ERR_NOT_SUPPORTED.
 *       Use spi_nand_flash_init_with_layers() instead.
 */
esp_err_t spi_nand_flash_init_device(spi_nand_flash_config_t *config, spi_nand_flash_device_t **handle);

//-----------------------------------------------------------------------------
// Page API (preferred terminology; NAND flash is page-based)
//-----------------------------------------------------------------------------

/** @brief Read a page from the nand flash.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] buffer The output buffer to put the read data into (must hold at least page_size bytes).
 * @param page_id Logical page index to read.
 * @return ESP_OK on success, or a flash error code if the read failed.
 */
esp_err_t spi_nand_flash_read_page(spi_nand_flash_device_t *handle, uint8_t *buffer, uint32_t page_id);

/** @brief Write a page to the nand flash.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param buffer The input buffer containing the data to write (must hold at least page_size bytes).
 * @param page_id Logical page index to write.
 * @return ESP_OK on success, or a flash error code if the write failed.
 */
esp_err_t spi_nand_flash_write_page(spi_nand_flash_device_t *handle, const uint8_t *buffer, uint32_t page_id);

/** @brief Copy a page to another page within the nand flash.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param src_page Source logical page index.
 * @param dst_page Destination logical page index.
 * @return ESP_OK on success, or a flash error code if the copy failed.
 */
esp_err_t spi_nand_flash_copy_page(spi_nand_flash_device_t *handle, uint32_t src_page, uint32_t dst_page);

/** @brief Trim a page from the nand flash.
 *
 * Marks the specified logical page as free to optimize memory usage and support wear-leveling.
 * Typically invoked when files are deleted or resized.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param page_id Logical page index to trim.
 * @return ESP_OK on success, or a flash error code if the trim failed.
 */
esp_err_t spi_nand_flash_trim(spi_nand_flash_device_t *handle, uint32_t page_id);

/** @brief Get the number of logical pages (capacity).
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] number_of_pages Pointer to store the total number of logical pages.
 * @return ESP_OK on success, or a flash error code if the operation failed.
 */
esp_err_t spi_nand_flash_get_page_count(spi_nand_flash_device_t *handle, uint32_t *number_of_pages);

/** @brief Get the size of each logical page in bytes.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] page_size Pointer to store the page size in bytes.
 * @return ESP_OK on success, or a flash error code if the operation failed.
 */
esp_err_t spi_nand_flash_get_page_size(spi_nand_flash_device_t *handle, uint32_t *page_size);

//-----------------------------------------------------------------------------
// Sector API (backward-compatible aliases; equivalent to page API)
//-----------------------------------------------------------------------------

/** @brief Read a sector (alias for spi_nand_flash_read_page).
 * @deprecated Use spi_nand_flash_read_page() for new code. Sector and page are equivalent in this API.
 */
esp_err_t spi_nand_flash_read_sector(spi_nand_flash_device_t *handle, uint8_t *buffer, uint32_t sector_id);

/** @brief Copy a sector (alias for spi_nand_flash_copy_page).
 * @deprecated Use spi_nand_flash_copy_page() for new code.
 */
esp_err_t spi_nand_flash_copy_sector(spi_nand_flash_device_t *handle, uint32_t src_sec, uint32_t dst_sec);

/** @brief Write a sector (alias for spi_nand_flash_write_page).
 * @deprecated Use spi_nand_flash_write_page() for new code.
 */
esp_err_t spi_nand_flash_write_sector(spi_nand_flash_device_t *handle, const uint8_t *buffer, uint32_t sector_id);

/** @brief Get number of sectors (alias for spi_nand_flash_get_page_count).
 * @deprecated Use spi_nand_flash_get_page_count() for new code.
 */
esp_err_t spi_nand_flash_get_capacity(spi_nand_flash_device_t *handle, uint32_t *number_of_sectors);

/** @brief Get sector size (alias for spi_nand_flash_get_page_size).
 * @deprecated Use spi_nand_flash_get_page_size() for new code.
 */
esp_err_t spi_nand_flash_get_sector_size(spi_nand_flash_device_t *handle, uint32_t *sector_size);

/** @brief Synchronizes any cache to the device.
 *
 * After this method is called, the nand flash chip should be synchronized with the results of any previous read/writes.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @return ESP_OK on success, or a flash error code if the synchronization failed.
 */
esp_err_t spi_nand_flash_sync(spi_nand_flash_device_t *handle);

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

/** @brief Perform explicit garbage collection step
 *
 * This function triggers one garbage collection step in the wear-leveling layer.
 * It reclaims blocks with garbage pages by copying valid data and erasing physical blocks.
 *
 * Note: Garbage collection happens automatically during write operations based on
 * the gc_factor setting. This function is useful when you want to proactively
 * reclaim space during idle time.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @return ESP_OK on success, or a flash error code if the operation failed.
 */
esp_err_t spi_nand_flash_gc(spi_nand_flash_device_t *handle);

/** @brief De-initialize the handle, releasing any resources reserved.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @return ESP_OK on success, or a flash error code if the de-initialization failed.
 */
esp_err_t spi_nand_flash_deinit_device(spi_nand_flash_device_t *handle);

//---------------------------------------------------------------------------------------------------------------------------------------------
// NEW LAYERED ARCHITECTURE API
//---------------------------------------------------------------------------------------------------------------------------------------------

#ifdef CONFIG_NAND_FLASH_ENABLE_BDL

/** @brief Initialize SPI NAND Flash with separate layer block devices
 *
 * This function provides direct access to the layered architecture, allowing
 * users to work with the flash and wear-leveling layers separately.
 * Both layers are exposed as standard esp_blockdev_t interfaces.
 *
 * @param config Configuration for the SPI NAND flash
 * @param[out] wl_bdl Pointer to store the Wear-Leveling Block Device Layer handle
 * @return
 *         - ESP_OK: Success
 *         - ESP_ERR_INVALID_ARG: Invalid configuration or NULL pointers
 *         - ESP_ERR_NO_MEM: Insufficient memory
 *         - ESP_ERR_NOT_FOUND: NAND device not detected
 */
esp_err_t spi_nand_flash_init_with_layers(spi_nand_flash_config_t *config,
                                          esp_blockdev_handle_t *wl_bdl);
#endif // CONFIG_NAND_FLASH_ENABLE_BDL

#ifdef __cplusplus
}
#endif
