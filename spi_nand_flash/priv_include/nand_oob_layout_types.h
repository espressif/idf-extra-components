/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2026 Espressif Systems (Shanghai) CO LTD
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Max disjoint free programmable regions cached for xfer (RFC MAX_REG). */
#define SPI_NAND_OOB_MAX_REGIONS 8

/**
 * @brief One spare-area slice in byte offsets from the start of OOB (column page_size + offset).
 *
 * Offsets are within the OOB area only (0 = first spare byte). Plane selection stays in nand_impl:
 * column on the wire is get_column_address(...) + page_size + offset, not encoded here.
 */
typedef struct {
    uint16_t offset;
    uint16_t length;
    bool programmable;
    /** True if internal ECC covers these bytes the same way as today's marker programs. */
    bool ecc_protected;
} spi_nand_oob_region_desc_t;

struct spi_nand_ooblayout_ops;
typedef struct spi_nand_ooblayout_ops spi_nand_ooblayout_ops_t;

/**
 * @brief Bad-block marker placement and "good block" pattern (separate from free_region list).
 *
 * check_pages_mask uses SPI_NAND_BBM_CHECK_*; matches first-page BBM read used by nand_is_bad today.
 */
typedef struct {
    uint16_t bbm_offset;
    uint16_t bbm_length;
    uint8_t good_pattern[2];
    uint32_t check_pages_mask;
} spi_nand_oob_bbm_t;

/**
 * @brief OOB layout: spare length hint, BBM policy, and ops to enumerate non-BBM regions.
 *
 * oob_bytes: user-visible spare length per page. Value 0 means "fill from chip/emulator at init"
 * (see default layout implementation notes).
 */
typedef struct spi_nand_oob_layout {
    uint8_t oob_bytes;
    spi_nand_oob_bbm_t bbm;
    const spi_nand_ooblayout_ops_t *ops;
} spi_nand_oob_layout_t;

/**
 * @brief Layout callbacks (chip_ctx is opaque; default layout ignores it).
 *
 * free_region must NOT return BBM bytes or raw ECC parity-only regions—only user-free programmable
 * spare. Exhausted sections return ESP_ERR_NOT_FOUND.
 */
struct spi_nand_ooblayout_ops {
    esp_err_t (*free_region)(const void *chip_ctx, int section, spi_nand_oob_region_desc_t *out);
    /** Optional parity/reserved enumeration; may be NULL. */
    esp_err_t (*ecc_region)(const void *chip_ctx, int section, spi_nand_oob_region_desc_t *out);
};

/** Which physical pages of a block participate in BBM checks (bitmask, ONFI-style intent). */
typedef enum {
    SPI_NAND_BBM_CHECK_FIRST_PAGE = 1u << 0,
    SPI_NAND_BBM_CHECK_LAST_PAGE = 1u << 1,
} spi_nand_bbm_check_pages_t;

/** Init-time field slots; runtime nand_* uses cached offsets, not per-call enum dispatch. */
typedef enum {
    SPI_NAND_OOB_FIELD_PAGE_USED = 0,
} spi_nand_oob_field_id_t;

/** Logical stream class for assigning PAGE_USED and similar into free spare. */
typedef enum {
    SPI_NAND_OOB_CLASS_FREE_ECC,
    SPI_NAND_OOB_CLASS_FREE_NOECC,
} spi_nand_oob_class_t;

typedef struct {
    spi_nand_oob_field_id_t id;
    uint8_t length;
    /** Logical stream (FREE_ECC vs FREE_NOECC); not the C++ `class` keyword. */
    spi_nand_oob_class_t oob_class;
    /** Byte offset within the concatenation of free regions of that class (set at init). */
    uint16_t logical_offset;
    bool assigned;
} spi_nand_oob_field_spec_t;

/**
 * @brief Per-call xfer state (implementation fills regs[] in step 04); keep stack-local for BDL paths.
 */
typedef struct {
    const spi_nand_oob_layout_t *layout;
    spi_nand_oob_class_t cls;
    uint8_t *oob_raw;
    uint16_t oob_size;
    spi_nand_oob_region_desc_t regs[SPI_NAND_OOB_MAX_REGIONS];
    uint8_t reg_count;
    size_t total_logical_len;
} spi_nand_oob_xfer_ctx_t;

#ifdef __cplusplus
}
#endif
