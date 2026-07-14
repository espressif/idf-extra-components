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
 * @brief Convert a 32-bit value between big-endian (on-flash) and host order.
 *
 * Self-inverse: use for both loading a field read from flash into host order and
 * for storing a host value into a header before writing it back to flash.
 *
 * @param[in] v  Value to convert.
 * @return Converted value.
 */
static inline uint32_t nand_ubi_be32(uint32_t v)
{
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return v;
#else
    return __builtin_bswap32(v);
#endif
}

/**
 * @brief Convert a 64-bit value between big-endian (on-flash) and host order.
 *
 * @see nand_ubi_be32
 */
static inline uint64_t nand_ubi_be64(uint64_t v)
{
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return v;
#else
    return __builtin_bswap64(v);
#endif
}

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
