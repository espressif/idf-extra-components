/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "spi_nand_flash_test_helpers.h"

#define SPI_NAND_FLASH_PATTERN_SEED  0x12345678U

void spi_nand_flash_fill_buffer(uint8_t *dst, size_t count)
{
    uint32_t *p = (uint32_t *)dst;
    for (size_t i = 0; i < count; ++i) {
        p[i] = SPI_NAND_FLASH_PATTERN_SEED + (uint32_t)i;
    }
}

void spi_nand_flash_fill_buffer_seeded(uint8_t *dst, size_t count, uint32_t seed)
{
    uint32_t *p = (uint32_t *)dst;
    for (size_t i = 0; i < count; ++i) {
        p[i] = seed + (uint32_t)i;
    }
}

int spi_nand_flash_check_buffer(const uint8_t *src, size_t count)
{
    const uint32_t *p = (const uint32_t *)src;
    for (size_t i = 0; i < count; ++i) {
        if (p[i] != SPI_NAND_FLASH_PATTERN_SEED + (uint32_t)i) {
            return (int)(i + 1);
        }
    }
    return 0;
}

int spi_nand_flash_check_buffer_seeded(const uint8_t *src, size_t count, uint32_t seed)
{
    const uint32_t *p = (const uint32_t *)src;
    for (size_t i = 0; i < count; ++i) {
        if (p[i] != seed + (uint32_t)i) {
            return (int)(i + 1);
        }
    }
    return 0;
}
