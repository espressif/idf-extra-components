/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Fill a buffer with a deterministic uint32_t pattern (for testing). */
void spi_nand_flash_fill_buffer(uint8_t *dst, size_t count);

/**
 * Check buffer against the same deterministic pattern.
 * @return 0 on match, 1-based index of first mismatch on failure.
 */
int spi_nand_flash_check_buffer(const uint8_t *src, size_t count);

#ifdef __cplusplus
}
#endif
