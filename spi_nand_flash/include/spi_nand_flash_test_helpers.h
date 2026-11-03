/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** ONFI parameter page size (bytes) used by host CRC golden-vector tests. */
#define SPI_NAND_TEST_ONFI_PARAM_PAGE_SIZE 256

#ifdef __cplusplus
extern "C" {
#endif

/** Fill a buffer with a deterministic uint32_t pattern (for testing). */
void spi_nand_flash_fill_buffer(uint8_t *dst, size_t count);

/**
 * @brief Fill buffer with a deterministic pattern using a caller-supplied seed.
 *
 * Pattern: word[i] = seed + i (uint32_t words). Use a different seed per write
 * round to produce distinct data across multiple overwrites of the same sector.
 *
 * @param dst   Destination buffer (must be 4-byte aligned, count * 4 bytes)
 * @param count Number of uint32_t words to fill
 * @param seed  Pattern seed value
 */
void spi_nand_flash_fill_buffer_seeded(uint8_t *dst, size_t count, uint32_t seed);

/**
 * Check buffer against the same deterministic pattern.
 * @return 0 on match, 1-based index of first mismatch on failure.
 */
int spi_nand_flash_check_buffer(const uint8_t *src, size_t count);

/**
 * Check buffer against spi_nand_flash_fill_buffer_seeded(@p seed).
 * @return 0 on match, 1-based index of first mismatch on failure.
 */
int spi_nand_flash_check_buffer_seeded(const uint8_t *src, size_t count, uint32_t seed);

/** ONFI parameter page CRC-16 (polynomial 0x8005, init 0x4F4E) over @p data[0..length-1]. */
uint16_t spi_nand_test_onfi_param_page_crc16(const uint8_t *data, size_t length);

/** Validate bytes 0-253 against CRC in bytes 254-255 (little-endian). */
bool spi_nand_test_onfi_param_page_crc_valid(const uint8_t *page_data, size_t page_size);

#ifdef __cplusplus
}
#endif
