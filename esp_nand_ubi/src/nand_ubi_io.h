/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "nand_ubi_media.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compute the UBI header CRC over a buffer.
 *
 * Matches the CRC used by Linux UBI for @c hdr_crc: standard CRC-32
 * (poly 0xEDB88320, reflected) seeded with @ref UBI_CRC32_INIT and with no
 * final inversion, so results are byte-compatible with @c mtd-utils @c ubinize.
 *
 * @param[in] buf  Input buffer.
 * @param[in] len  Number of bytes to hash.
 * @return CRC-32 value.
 */
uint32_t nand_ubi_crc32(const void *buf, uint32_t len);

/**
 * @brief Validate an EC header read from flash.
 *
 * Checks the magic number and verifies @c hdr_crc over the leading bytes
 * (all fields except @c hdr_crc itself).
 *
 * @param[in] h  Pointer to a 64-byte EC header in host-accessible memory.
 * @return true if the header is structurally valid.
 */
bool nand_ubi_ec_hdr_valid(const nand_ubi_ec_hdr_t *h);

/**
 * @brief Validate a VID header read from flash.
 *
 * Checks the magic number and verifies @c hdr_crc over the leading bytes
 * (all fields except @c hdr_crc itself).
 *
 * @param[in] h  Pointer to a 64-byte VID header in host-accessible memory.
 * @return true if the header is structurally valid.
 */
bool nand_ubi_vid_hdr_valid(const nand_ubi_vid_hdr_t *h);

#ifdef __cplusplus
}
#endif
