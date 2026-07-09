/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * On-flash UBI header layout. Byte-for-byte compatible with the Linux kernel
 * UBI headers (drivers/mtd/ubi/ubi-media.h) so that images produced by
 * mtd-utils "ubinize" are accepted by this layer. All multi-byte fields are
 * stored big-endian on flash; use the be16/be32/be64 helpers to access them.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UBI_EC_HDR_MAGIC   0x55424923u   /* ASCII "UBI#" */
#define UBI_VID_HDR_MAGIC  0x55424921u   /* ASCII "UBI!" */

#define UBI_VERSION        1

#define UBI_CRC32_INIT     0xFFFFFFFFu

#define UBI_VID_DYNAMIC    1
#define UBI_VID_STATIC     2

#define UBI_LEB_UNMAPPED   ((int32_t)(-1))
#define UBI_FREE_LEB       ((uint64_t)(~0ULL))

/**
 * @brief On-flash UBI erase-counter header (64 bytes).
 *
 * Layout matches @c struct ubi_ec_hdr from the Linux kernel exactly. Stored at
 * offset 0 of every physical erase block. All multi-byte fields are big-endian.
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;             /*!< off 0:  UBI_EC_HDR_MAGIC (big-endian). */
    uint8_t  version;           /*!< off 4:  UBI_VERSION. */
    uint8_t  padding1[3];       /*!< off 5:  reserved, zeroes. */
    uint64_t ec;                /*!< off 8:  erase counter (big-endian). */
    uint32_t vid_hdr_offset;    /*!< off 16: VID header offset within PEB (big-endian). */
    uint32_t data_offset;       /*!< off 20: user data offset within PEB (big-endian). */
    uint32_t image_seq;         /*!< off 24: image sequence number (big-endian). */
    uint8_t  padding2[32];      /*!< off 28: reserved, zeroes. */
    uint32_t hdr_crc;           /*!< off 60: CRC over bytes [0,60) (big-endian). */
} nand_ubi_ec_hdr_t;

/**
 * @brief On-flash UBI volume-identifier header (64 bytes).
 *
 * Layout matches @c struct ubi_vid_hdr from the Linux kernel exactly. Stored at
 * @c vid_hdr_offset of every mapped physical erase block. Multi-byte fields are
 * big-endian.
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;             /*!< off 0:  UBI_VID_HDR_MAGIC (big-endian). */
    uint8_t  version;           /*!< off 4:  UBI_VERSION. */
    uint8_t  vol_type;          /*!< off 5:  UBI_VID_DYNAMIC or UBI_VID_STATIC. */
    uint8_t  copy_flag;         /*!< off 6:  set if this LEB is a wear-leveling copy. */
    uint8_t  compat;            /*!< off 7:  compatibility (0 for user volumes). */
    uint32_t vol_id;            /*!< off 8:  volume ID (big-endian). */
    uint32_t lnum;              /*!< off 12: logical erase block number (big-endian). */
    uint8_t  padding1[4];       /*!< off 16: reserved, zeroes. */
    uint32_t data_size;         /*!< off 20: bytes of data in this LEB (big-endian). */
    uint32_t used_ebs;          /*!< off 24: used LEBs in this volume, static vols (big-endian). */
    uint32_t data_pad;          /*!< off 28: unused bytes at end of PEB (big-endian). */
    uint32_t data_crc;          /*!< off 32: CRC of LEB data, when copy_flag set (big-endian). */
    uint8_t  padding2[4];       /*!< off 36: reserved, zeroes. */
    uint64_t sqnum;             /*!< off 40: global sequence number (big-endian). */
    uint8_t  padding3[12];      /*!< off 48: reserved, zeroes. */
    uint32_t hdr_crc;           /*!< off 60: CRC over bytes [0,60) (big-endian). */
} nand_ubi_vid_hdr_t;

_Static_assert(sizeof(nand_ubi_ec_hdr_t) == 64, "EC header must be exactly 64 bytes");
_Static_assert(sizeof(nand_ubi_vid_hdr_t) == 64, "VID header must be exactly 64 bytes");
_Static_assert(offsetof(nand_ubi_ec_hdr_t, ec) == 8, "EC header ec field misaligned");
_Static_assert(offsetof(nand_ubi_ec_hdr_t, image_seq) == 24, "EC header image_seq field misaligned");
_Static_assert(offsetof(nand_ubi_ec_hdr_t, hdr_crc) == 60, "EC header hdr_crc field misaligned");
_Static_assert(offsetof(nand_ubi_vid_hdr_t, vol_id) == 8, "VID header vol_id field misaligned");
_Static_assert(offsetof(nand_ubi_vid_hdr_t, lnum) == 12, "VID header lnum field misaligned");
_Static_assert(offsetof(nand_ubi_vid_hdr_t, sqnum) == 40, "VID header sqnum field misaligned");
_Static_assert(offsetof(nand_ubi_vid_hdr_t, hdr_crc) == 60, "VID header hdr_crc field misaligned");

/** @brief Number of header bytes covered by @c hdr_crc (all fields but the trailing CRC). */
#define UBI_EC_HDR_SIZE_CRC   ((uint32_t)offsetof(nand_ubi_ec_hdr_t, hdr_crc))
#define UBI_VID_HDR_SIZE_CRC  ((uint32_t)offsetof(nand_ubi_vid_hdr_t, hdr_crc))

#ifdef __cplusplus
}
#endif
