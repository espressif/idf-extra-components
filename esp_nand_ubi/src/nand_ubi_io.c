/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "nand_ubi_io.h"
#include "esp_rom_crc.h"

uint32_t nand_ubi_crc32(const void *buf, size_t len)
{
    /*
     * Linux UBI computes hdr_crc as crc32(UBI_CRC32_INIT, buf, len) via the
     * kernel crc32_le(), which seeds with 0xFFFFFFFF but does NOT apply the
     * trailing bit inversion. esp_rom_crc32_le() applies both the leading and
     * trailing inversion, so its result must be inverted once more to match the
     * UBI value byte-for-byte. Verified empirically against zlib and a reference
     * kernel crc32_le implementation.
     */
    return ~esp_rom_crc32_le(0, (const uint8_t *)buf, (uint32_t)len);
}

static inline uint32_t be32_load(uint32_t v)
{
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return v;
#else
    return __builtin_bswap32(v);
#endif
}

bool nand_ubi_ec_hdr_valid(const nand_ubi_ec_hdr_t *h)
{
    if (be32_load(h->magic) != UBI_EC_HDR_MAGIC) {
        return false;
    }
    return be32_load(h->hdr_crc) == nand_ubi_crc32(h, UBI_EC_HDR_SIZE_CRC);
}

bool nand_ubi_vid_hdr_valid(const nand_ubi_vid_hdr_t *h)
{
    if (be32_load(h->magic) != UBI_VID_HDR_MAGIC) {
        return false;
    }
    return be32_load(h->hdr_crc) == nand_ubi_crc32(h, UBI_VID_HDR_SIZE_CRC);
}
