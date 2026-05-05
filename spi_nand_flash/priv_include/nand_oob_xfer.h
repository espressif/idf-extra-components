/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2026 Espressif Systems (Shanghai) CO LTD
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "nand_oob_layout_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup nand_oob_xfer OOB logical ↔ physical scatter/gather
 *
 * Logical offsets are a contiguous index space over the layout’s **free_region** slices that match
 * the requested @ref spi_nand_oob_class_t (concatenated in enumeration order). Physical placement is
 * `oob_raw[region.offset + k]` where `region.offset` is within the spare area (0 = first OOB byte
 * at column `page_size`); BBM bytes live outside the free stream and must never be reached via a
 * valid logical range.
 *
 * **Single `program_execute` per logical page program (proposal §2.2):** These helpers only copy
 * bytes in RAM (`oob_raw` / caller buffers). They do **not** issue NAND I/O. Callers that split one
 * logical program into multiple `program_execute` steps must not use scatter output that way; all
 * `program_load` sequences derived from one scatter pass for a page must be followed by exactly one
 * `program_execute_and_wait` (enforced in `nand_prog` / related paths, not here).
 */

esp_err_t nand_oob_xfer_ctx_init(spi_nand_oob_xfer_ctx_t *ctx,
                                 const spi_nand_oob_layout_t *layout,
                                 const void *chip_ctx,
                                 spi_nand_oob_class_t cls,
                                 uint8_t *oob_raw,
                                 uint16_t oob_size);

esp_err_t nand_oob_gather(const spi_nand_oob_xfer_ctx_t *ctx,
                          size_t logical_off,
                          void *dst,
                          size_t len);

esp_err_t nand_oob_scatter(spi_nand_oob_xfer_ctx_t *ctx,
                           size_t logical_off,
                           const void *src,
                           size_t len);

#ifdef __cplusplus
}
#endif
