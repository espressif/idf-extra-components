/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/**
 * @file esp_nand_ubi.h
 * @brief UBI-like Block Device Layer (BDL) middleware for SPI NAND flash.
 *
 * This component wraps a raw SPI NAND flash BDL (from @c nand_flash_get_blockdev())
 * and exposes a flat logical-erase-block (LEB) address space per volume. It hides
 * factory and runtime bad blocks and makes factory images position-independent:
 * the filesystem above only ever sees LEB numbers, never physical PEB numbers.
 *
 * The API is split into:
 * - device-level (@c nand_ubi_attach / @c nand_ubi_detach) — once per physical chip;
 * - volume-level (@c nand_ubi_open_volume) — once per logical region;
 * - a convenience wrapper (@c nand_ubi_get_blockdev) for the single-volume case.
 *
 * @note The underlying handle passed to @c nand_ubi_attach() must be the RAW flash
 *       BDL (@c nand_flash_get_blockdev()), NOT the Dhara wear-leveling BDL
 *       (@c spi_nand_flash_wl_get_blockdev()). Stacking UBI on top of Dhara would
 *       result in a double FTL with incompatible geometry contracts.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_blockdev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration passed at attach time.
 */
typedef struct {
    uint32_t reserved_pebs;   /**< Spare PEBs held back for the bad-block pool (default 4). */
    bool     read_only;       /**< Attach without modifying flash (image inspection). */
} nand_ubi_config_t;

/**
 * @brief Default-initializer for @ref nand_ubi_config_t.
 */
#define NAND_UBI_CONFIG_DEFAULT() { .reserved_pebs = 4, .read_only = false }

/**
 * @brief Opaque handle representing an attached UBI device (one physical NAND chip).
 */
typedef struct nand_ubi_device nand_ubi_device_t;

/* ── device-level ──────────────────────────────────────────────────────── */

/**
 * @brief Scan all PEBs and build the EBA table for a raw NAND BDL.
 *
 * One call per physical device, regardless of volume count. Reads only the
 * EC and VID headers (2 pages per PEB); the temporary page buffer is freed
 * after the scan completes.
 *
 * @param[in]  nand_bdl     Raw flash BDL from @c nand_flash_get_blockdev().
 *                          Must NOT be the Dhara wear-leveling BDL.
 * @param[in]  config       Attach config (NULL selects defaults).
 * @param[out] out_ubi_dev  Output: opaque device handle.
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p nand_bdl or @p out_ubi_dev is NULL
 *      - ESP_ERR_NO_MEM if allocation of the EBA/state tables failed
 *      - another @c esp_err_t error code on I/O failure
 */
esp_err_t nand_ubi_attach(esp_blockdev_handle_t    nand_bdl,
                          const nand_ubi_config_t *config,
                          nand_ubi_device_t      **out_ubi_dev);

/**
 * @brief Release all resources held by a UBI device.
 *
 * All volume BDL handles opened from this device must be released first via
 * @c vol_bdl->ops->release(vol_bdl). This does NOT release the underlying
 * @p nand_bdl passed to @c nand_ubi_attach().
 *
 * @param[in] ubi_dev  Device handle from @c nand_ubi_attach().
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p ubi_dev is NULL
 *      - ESP_ERR_INVALID_STATE if volumes are still open
 */
esp_err_t nand_ubi_detach(nand_ubi_device_t *ubi_dev);

/* ── volume-level ──────────────────────────────────────────────────────── */

/**
 * @brief Open one volume and return a BDL handle scoped to it.
 *
 * Phase 1: only @p vol_id = 0 is valid (whole chip = one volume, no volume table).
 * Phase 3: any @p vol_id present in the volume table.
 *
 * The returned handle must be released via @c vol_bdl->ops->release(vol_bdl)
 * before @c nand_ubi_detach() is called.
 *
 * @param[in]  ubi_dev      Device handle from @c nand_ubi_attach().
 * @param[in]  vol_id       Volume ID (0 in Phase 1).
 * @param[out] out_vol_bdl  Output: BDL handle for this volume.
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on NULL arguments
 *      - ESP_ERR_NOT_FOUND if @p vol_id does not exist
 *      - ESP_ERR_NO_MEM if allocation of the volume wrapper failed
 */
esp_err_t nand_ubi_open_volume(nand_ubi_device_t     *ubi_dev,
                               uint32_t               vol_id,
                               esp_blockdev_handle_t *out_vol_bdl);

/* ── convenience wrapper (single-volume common case) ───────────────────── */

/**
 * @brief attach + open_volume(0) in one call.
 *
 * Equivalent to @c nand_ubi_attach() followed by @c nand_ubi_open_volume(dev, 0, ...).
 * The @ref nand_ubi_device_t lifecycle is hidden inside the returned BDL; calling
 * @c vol_bdl->ops->release(vol_bdl) also detaches the device.
 *
 * Use this when the whole NAND is one filesystem and no multi-volume
 * management is needed.
 *
 * @param[in]  nand_bdl     Raw flash BDL from @c nand_flash_get_blockdev().
 * @param[in]  config       Attach config (NULL selects defaults).
 * @param[out] out_vol_bdl  Output: BDL handle for volume 0.
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on NULL arguments
 *      - ESP_ERR_NO_MEM on allocation failure
 *      - another @c esp_err_t error code on I/O failure
 */
esp_err_t nand_ubi_get_blockdev(esp_blockdev_handle_t    nand_bdl,
                                const nand_ubi_config_t *config,
                                esp_blockdev_handle_t   *out_vol_bdl);

#ifdef __cplusplus
}
#endif
