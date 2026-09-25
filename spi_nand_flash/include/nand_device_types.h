/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief NAND Flash ECC status enumeration
 *
 * Values are distinct IDs, not raw hardware ECC status register bit patterns. Every
 * raw-to-status mapping goes through an explicit per-chip lookup table, so compare by
 * name only. The pre-existing values keep their original numbers for compatibility.
 */
typedef enum {
    NAND_ECC_OK = 0,                       /*!< No ECC errors detected */
    NAND_ECC_1_TO_3_BITS_CORRECTED = 1,    /*!< 1-3 bits corrected */
    NAND_ECC_BITS_CORRECTED = NAND_ECC_1_TO_3_BITS_CORRECTED,
    NAND_ECC_NOT_CORRECTED = 2,            /*!< ECC errors not correctable */
    NAND_ECC_4_TO_6_BITS_CORRECTED = 3,    /*!< 4-6 bits corrected */
    NAND_ECC_MAX_BITS_CORRECTED = NAND_ECC_4_TO_6_BITS_CORRECTED,
    NAND_ECC_7_8_BITS_CORRECTED = 5,       /*!< 7-8 bits corrected */
    NAND_ECC_1_TO_4_BITS_CORRECTED,        /*!< 1-4 bits corrected */
    NAND_ECC_1_BIT_CORRECTED,              /*!< exactly 1 bit corrected */
    NAND_ECC_2_BITS_CORRECTED,             /*!< exactly 2 bits corrected */
    NAND_ECC_3_BITS_CORRECTED,             /*!< exactly 3 bits corrected */
    NAND_ECC_4_BITS_CORRECTED,             /*!< exactly 4 bits corrected */
    NAND_ECC_5_BITS_CORRECTED,             /*!< exactly 5 bits corrected */
    NAND_ECC_6_BITS_CORRECTED,             /*!< exactly 6 bits corrected */
    NAND_ECC_7_BITS_CORRECTED,             /*!< exactly 7 bits corrected */
    NAND_ECC_8_BITS_CORRECTED,             /*!< exactly 8 bits corrected */
    NAND_ECC_1_TO_7_BITS_CORRECTED,        /*!< 1-7 bits corrected */
    NAND_ECC_INVALID,                      /*!< Status could not be determined: the chip reported a reserved
                                                pattern, or decoding it failed. Treat as not corrected. */
    NAND_ECC_MAX                           /*!< One past the highest value (not a count: 4 is unused); not a status */
} nand_ecc_status_t;

/**
 * @brief Worst-case corrected-bit count for a status class (range ceiling).
 *
 * Refresh compares this to the tunable threshold. Using max (not min) may
 * refresh early if the threshold sits inside a range; using min can skip a
 * needed refresh.
 */
static inline uint8_t nand_ecc_max_bits_corrected(nand_ecc_status_t status)
{
    switch (status) {
    case NAND_ECC_1_TO_3_BITS_CORRECTED:
        return 3;
    case NAND_ECC_4_TO_6_BITS_CORRECTED:
        return 6;
    case NAND_ECC_7_8_BITS_CORRECTED:
        return 8;
    case NAND_ECC_1_TO_4_BITS_CORRECTED:
        return 4;
    case NAND_ECC_1_BIT_CORRECTED:
        return 1;
    case NAND_ECC_2_BITS_CORRECTED:
        return 2;
    case NAND_ECC_3_BITS_CORRECTED:
        return 3;
    case NAND_ECC_4_BITS_CORRECTED:
        return 4;
    case NAND_ECC_5_BITS_CORRECTED:
        return 5;
    case NAND_ECC_6_BITS_CORRECTED:
        return 6;
    case NAND_ECC_7_BITS_CORRECTED:
        return 7;
    case NAND_ECC_8_BITS_CORRECTED:
        return 8;
    case NAND_ECC_1_TO_7_BITS_CORRECTED:
        return 7;
    case NAND_ECC_OK:
    case NAND_ECC_NOT_CORRECTED:
    case NAND_ECC_INVALID:
    case NAND_ECC_MAX:
        return 0;
    }
    /* No default: above, so -Wswitch flags any nand_ecc_status_t value missing here. */
    return 0;
}

/** @brief NAND Flash ECC configuration and status */
typedef struct {
    uint8_t ecc_status_reg_len_in_bits;     /*!< Deprecated: informational only, not used by the driver.
                                                 ECC status is decoded by a per-chip decoder. */
    uint8_t ecc_data_refresh_threshold;     /*!< ECC error threshold for data refresh */
    nand_ecc_status_t ecc_corrected_bits_status; /*!< Current ECC correction status */
} nand_ecc_data_t;

/** @brief NAND Flash chip geometry and characteristics */
typedef struct {
    uint8_t log2_page_size;                 /*!< Page size as power of 2 (e.g., 11 for 2048 bytes) */
    uint8_t log2_ppb;                       /*!< Pages per block as power of 2 (e.g., 6 for 64 pages) */
    uint32_t block_size;                    /*!< Block size in bytes */
    uint32_t page_size;                     /*!< Page size in bytes */
    uint32_t num_blocks;                    /*!< Total number of blocks */
    uint32_t read_page_delay_us;            /*!< Read page delay in microseconds */
    uint32_t erase_block_delay_us;          /*!< Erase block delay in microseconds */
    uint32_t program_page_delay_us;         /*!< Program page delay in microseconds */
    uint32_t num_planes;                    /*!< Number of planes in the flash */
    uint32_t flags;                         /*!< Chip-specific flags */
    nand_ecc_data_t ecc_data;              /*!< ECC configuration and status */
    uint8_t has_quad_enable_bit;           /*!< 1 if chip supports QIO/QOUT mode */
    uint8_t quad_enable_bit_pos;           /*!< Position of quad enable bit */
#ifdef CONFIG_IDF_TARGET_LINUX
    uint32_t emulated_page_size;            /*!< Emulated page size for Linux */
    uint32_t emulated_page_oob;             /*!< Emulated OOB size for Linux */
#endif
} nand_flash_geometry_t;

/** @brief NAND Flash device identification information */
typedef struct {
    uint8_t manufacturer_id;                /*!< Manufacturer ID */
    uint16_t device_id;                     /*!< Device ID */
    char chip_name[32];                     /*!< Chip name string */
} nand_device_info_t;

#ifdef __cplusplus
}
#endif
