/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <cstring>
#include <catch2/catch_test_macros.hpp>

#include "esp_nand_ubi.h"
#include "nand_ubi_test_helpers.h"

extern "C" {
#include "nand_ubi_media.h"
#include "nand_ubi_io.h"
#include "nand_ubi_eba.h"
}

using namespace nand_ubi_test;

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

TEST_CASE("EBA alloc and free", "[nand_ubi][eba]")
{
    nand_ubi_eba_t eba;
    REQUIRE(nand_ubi_eba_alloc(16, 16, &eba) == ESP_OK);
    REQUIRE(eba.eba != nullptr);
    REQUIRE(eba.peb_state != nullptr);
    REQUIRE(eba.peb_count == 16);
    REQUIRE(eba.leb_count == 16);
    nand_ubi_eba_free(&eba);
    REQUIRE(eba.eba == nullptr);
    REQUIRE(eba.peb_state == nullptr);
}

TEST_CASE("all PEBs free after alloc", "[nand_ubi][eba]")
{
    nand_ubi_eba_t eba;
    REQUIRE(nand_ubi_eba_alloc(8, 8, &eba) == ESP_OK);
    for (uint32_t pnum = 0; pnum < 8; pnum++) {
        REQUIRE(nand_ubi_eba_peb_is_free(&eba, pnum));
    }
    nand_ubi_eba_free(&eba);
}

TEST_CASE("all LEBs unmapped after alloc", "[nand_ubi][eba]")
{
    nand_ubi_eba_t eba;
    REQUIRE(nand_ubi_eba_alloc(8, 8, &eba) == ESP_OK);
    for (uint32_t lnum = 0; lnum < 8; lnum++) {
        REQUIRE(nand_ubi_eba_get_pnum(&eba, lnum) == UBI_LEB_UNMAPPED);
    }
    nand_ubi_eba_free(&eba);
}

TEST_CASE("EBA set and get round-trip", "[nand_ubi][eba]")
{
    nand_ubi_eba_t eba;
    REQUIRE(nand_ubi_eba_alloc(8, 8, &eba) == ESP_OK);
    nand_ubi_eba_set(&eba, 0, 3);
    nand_ubi_eba_set(&eba, 1, 7);
    REQUIRE(nand_ubi_eba_get_pnum(&eba, 0) == 3);
    REQUIRE(nand_ubi_eba_get_pnum(&eba, 1) == 7);
    REQUIRE(nand_ubi_eba_get_pnum(&eba, 2) == UBI_LEB_UNMAPPED);
    nand_ubi_eba_free(&eba);
}

TEST_CASE("PEB state transitions: free to used to free", "[nand_ubi][eba]")
{
    nand_ubi_eba_t eba;
    REQUIRE(nand_ubi_eba_alloc(4, 4, &eba) == ESP_OK);
    REQUIRE(nand_ubi_eba_peb_is_free(&eba, 2));
    nand_ubi_eba_peb_set_used(&eba, 2);
    REQUIRE_FALSE(nand_ubi_eba_peb_is_free(&eba, 2));
    nand_ubi_eba_peb_set_free(&eba, 2);
    REQUIRE(nand_ubi_eba_peb_is_free(&eba, 2));
    nand_ubi_eba_free(&eba);
}

TEST_CASE("bad PEB is not free", "[nand_ubi][eba]")
{
    nand_ubi_eba_t eba;
    REQUIRE(nand_ubi_eba_alloc(4, 4, &eba) == ESP_OK);
    nand_ubi_eba_peb_set_bad(&eba, 1);
    REQUIRE_FALSE(nand_ubi_eba_peb_is_free(&eba, 1));
    REQUIRE(nand_ubi_eba_peb_is_free(&eba, 0));
    REQUIRE(nand_ubi_eba_peb_is_free(&eba, 2));
    nand_ubi_eba_free(&eba);
}

TEST_CASE("erase-pending PEB is not free", "[nand_ubi][eba]")
{
    nand_ubi_eba_t eba;
    REQUIRE(nand_ubi_eba_alloc(4, 4, &eba) == ESP_OK);
    nand_ubi_eba_peb_set_used(&eba, 1);
    nand_ubi_eba_peb_set_erase_pending(&eba, 1);
    REQUIRE_FALSE(nand_ubi_eba_peb_is_free(&eba, 1));
    REQUIRE(nand_ubi_eba_peb_is_free(&eba, 0));
    REQUIRE(nand_ubi_eba_peb_is_free(&eba, 2));
    nand_ubi_eba_free(&eba);
}

TEST_CASE("find_free_peb skips bad and used blocks", "[nand_ubi][eba]")
{
    nand_ubi_eba_t eba;
    REQUIRE(nand_ubi_eba_alloc(8, 8, &eba) == ESP_OK);
    nand_ubi_eba_peb_set_bad(&eba, 0);
    nand_ubi_eba_peb_set_used(&eba, 1);
    nand_ubi_eba_peb_set_bad(&eba, 2);
    REQUIRE(nand_ubi_eba_find_free_peb(&eba, 8) == 3);
    nand_ubi_eba_free(&eba);
}

TEST_CASE("find_free_peb returns -1 when no free PEB exists", "[nand_ubi][eba]")
{
    nand_ubi_eba_t eba;
    REQUIRE(nand_ubi_eba_alloc(4, 4, &eba) == ESP_OK);
    for (uint32_t pnum = 0; pnum < 4; pnum++) {
        nand_ubi_eba_peb_set_used(&eba, pnum);
    }
    REQUIRE(nand_ubi_eba_find_free_peb(&eba, 4) == -1);
    nand_ubi_eba_free(&eba);
}

TEST_CASE("peb_state 2-bit packing does not bleed between neighbours", "[nand_ubi][eba]")
{
    nand_ubi_eba_t eba;
    REQUIRE(nand_ubi_eba_alloc(16, 16, &eba) == ESP_OK);
    for (uint32_t pnum = 0; pnum < 16; pnum += 2) {
        nand_ubi_eba_peb_set_bad(&eba, pnum);
    }
    for (uint32_t pnum = 0; pnum < 16; pnum++) {
        if (pnum % 2 == 0) {
            REQUIRE_FALSE(nand_ubi_eba_peb_is_free(&eba, pnum));
        } else {
            REQUIRE(nand_ubi_eba_peb_is_free(&eba, pnum));
        }
    }
    nand_ubi_eba_free(&eba);
}
