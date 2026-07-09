/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <cstring>
#include <catch2/catch_test_macros.hpp>

#include "esp_nand_ubi.h"

extern "C" {
#include "nand_ubi_media.h"
#include "nand_ubi_io.h"
}

namespace {

static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
              "test helpers assume a little-endian build host");

uint32_t to_be32(uint32_t v)
{
    return __builtin_bswap32(v);
}

uint64_t to_be64(uint64_t v)
{
    return __builtin_bswap64(v);
}

void fill_ec_hdr(nand_ubi_ec_hdr_t *h, uint32_t image_seq,
                 uint32_t vid_hdr_offset, uint32_t data_offset)
{
    memset(h, 0, sizeof(*h));
    h->magic = to_be32(UBI_EC_HDR_MAGIC);
    h->version = UBI_VERSION;
    h->ec = to_be64(0);
    h->vid_hdr_offset = to_be32(vid_hdr_offset);
    h->data_offset = to_be32(data_offset);
    h->image_seq = to_be32(image_seq);
    h->hdr_crc = to_be32(nand_ubi_crc32(h, UBI_EC_HDR_SIZE_CRC));
}

void fill_vid_hdr(nand_ubi_vid_hdr_t *h, uint32_t vol_id, uint32_t lnum, uint64_t sqnum)
{
    memset(h, 0, sizeof(*h));
    h->magic = to_be32(UBI_VID_HDR_MAGIC);
    h->version = UBI_VERSION;
    h->vol_type = UBI_VID_DYNAMIC;
    h->vol_id = to_be32(vol_id);
    h->lnum = to_be32(lnum);
    h->sqnum = to_be64(sqnum);
    h->hdr_crc = to_be32(nand_ubi_crc32(h, UBI_VID_HDR_SIZE_CRC));
}

} // namespace

TEST_CASE("esp_nand_ubi public header is usable", "[nand_ubi][skeleton]")
{
    nand_ubi_config_t cfg = NAND_UBI_CONFIG_DEFAULT();
    REQUIRE(cfg.reserved_pebs == 4);
    REQUIRE(cfg.read_only == false);
}

TEST_CASE("on-flash header structs have the exact UBI layout", "[nand_ubi][media]")
{
    REQUIRE(sizeof(nand_ubi_ec_hdr_t) == 64);
    REQUIRE(sizeof(nand_ubi_vid_hdr_t) == 64);
    REQUIRE(offsetof(nand_ubi_ec_hdr_t, ec) == 8);
    REQUIRE(offsetof(nand_ubi_ec_hdr_t, image_seq) == 24);
    REQUIRE(offsetof(nand_ubi_ec_hdr_t, hdr_crc) == 60);
    REQUIRE(offsetof(nand_ubi_vid_hdr_t, vol_id) == 8);
    REQUIRE(offsetof(nand_ubi_vid_hdr_t, lnum) == 12);
    REQUIRE(offsetof(nand_ubi_vid_hdr_t, sqnum) == 40);
    REQUIRE(offsetof(nand_ubi_vid_hdr_t, hdr_crc) == 60);
}

TEST_CASE("nand_ubi_crc32 matches the Linux UBI CRC", "[nand_ubi][crc]")
{
    // Standard CRC-32 of "123456789" is 0xCBF43926; the UBI variant (kernel
    // crc32_le, no trailing inversion) is the bitwise complement: 0x340BC6D9.
    // This proves byte-for-byte compatibility with mtd-utils ubinize.
    const char *check = "123456789";
    REQUIRE(nand_ubi_crc32(check, (uint32_t)strlen(check)) == 0x340BC6D9u);
}

TEST_CASE("EC header CRC matches an independently computed ubinize value", "[nand_ubi][crc]")
{
    nand_ubi_ec_hdr_t h;
    fill_ec_hdr(&h, 0xABCD1234u, 2048, 4096);
    // Reference value computed offline from the same field values.
    REQUIRE(__builtin_bswap32(h.hdr_crc) == 0xCB7E5895u);
}

TEST_CASE("VID header CRC matches an independently computed ubinize value", "[nand_ubi][crc]")
{
    nand_ubi_vid_hdr_t h;
    fill_vid_hdr(&h, 0, 0, 1);
    REQUIRE(__builtin_bswap32(h.hdr_crc) == 0xAE2772D2u);
}

TEST_CASE("valid EC header is accepted", "[nand_ubi][validate]")
{
    nand_ubi_ec_hdr_t h;
    fill_ec_hdr(&h, 0xABCD1234u, 2048, 4096);
    REQUIRE(nand_ubi_ec_hdr_valid(&h));
}

TEST_CASE("EC header with wrong magic is rejected", "[nand_ubi][validate]")
{
    nand_ubi_ec_hdr_t h;
    fill_ec_hdr(&h, 0xABCD1234u, 2048, 4096);
    h.magic = to_be32(0xDEADBEEFu);
    REQUIRE_FALSE(nand_ubi_ec_hdr_valid(&h));
}

TEST_CASE("EC header with corrupted CRC is rejected", "[nand_ubi][validate]")
{
    nand_ubi_ec_hdr_t h;
    fill_ec_hdr(&h, 0xABCD1234u, 2048, 4096);
    h.image_seq = to_be32(0x00000001u); // change a covered field without fixing CRC
    REQUIRE_FALSE(nand_ubi_ec_hdr_valid(&h));
}

TEST_CASE("valid VID header is accepted", "[nand_ubi][validate]")
{
    nand_ubi_vid_hdr_t h;
    fill_vid_hdr(&h, 0, 7, 42);
    REQUIRE(nand_ubi_vid_hdr_valid(&h));
}

TEST_CASE("VID header with wrong magic is rejected", "[nand_ubi][validate]")
{
    nand_ubi_vid_hdr_t h;
    fill_vid_hdr(&h, 0, 7, 42);
    h.magic = to_be32(UBI_EC_HDR_MAGIC); // right family, wrong header type
    REQUIRE_FALSE(nand_ubi_vid_hdr_valid(&h));
}

TEST_CASE("VID header with corrupted CRC is rejected", "[nand_ubi][validate]")
{
    nand_ubi_vid_hdr_t h;
    fill_vid_hdr(&h, 0, 7, 42);
    h.lnum = to_be32(8);
    REQUIRE_FALSE(nand_ubi_vid_hdr_valid(&h));
}
