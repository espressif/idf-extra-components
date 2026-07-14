/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "esp_blockdev.h"
#include "esp_nand_blockdev.h"
#include "spi_nand_flash.h"
#include "nand_linux_mmap_emul.h"

extern "C" {
#include "nand_ubi_media.h"
#include "nand_ubi_io.h"
#include "nand_ubi_eba.h"
}

namespace nand_ubi_test {

static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
              "test helpers assume a little-endian build host");

inline uint32_t to_be32(uint32_t v)
{
    return __builtin_bswap32(v);
}

inline uint64_t to_be64(uint64_t v)
{
    return __builtin_bswap64(v);
}

inline void fill_ec_hdr(nand_ubi_ec_hdr_t *h, uint32_t image_seq,
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

/* copy_flag/data_size/data_crc default to the common (non-WL-copy) case; pass them
 * explicitly to build a fixture that exercises attach()'s copy_flag verification. */
inline void fill_vid_hdr(nand_ubi_vid_hdr_t *h, uint32_t vol_id, uint32_t lnum, uint64_t sqnum,
                         uint8_t copy_flag = 0, uint32_t data_size = 0, uint32_t data_crc = 0)
{
    memset(h, 0, sizeof(*h));
    h->magic = to_be32(UBI_VID_HDR_MAGIC);
    h->version = UBI_VERSION;
    h->vol_type = UBI_VID_DYNAMIC;
    h->copy_flag = copy_flag;
    h->vol_id = to_be32(vol_id);
    h->lnum = to_be32(lnum);
    h->sqnum = to_be64(sqnum);
    h->data_size = to_be32(data_size);
    h->data_crc = to_be32(data_crc);
    h->hdr_crc = to_be32(nand_ubi_crc32(h, UBI_VID_HDR_SIZE_CRC));
}

/* Fresh emulated NAND backed by its own temp file (flash_file_name=""), so parallel
 * TEST_CASEs never share storage. Geometry matches the sibling spi_nand_flash host
 * tests: 2 KiB pages, 64 pages/block -> 128 KiB blocks, matching the plan's worked example. */
inline esp_blockdev_handle_t make_test_nand(uint32_t file_bytes = 50u * 1024u * 1024u)
{
    nand_file_mmap_emul_config_t conf = {"", file_bytes, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t nand_bdl = nullptr;
    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &nand_bdl) == ESP_OK);
    REQUIRE(nand_bdl != nullptr);
    return nand_bdl;
}

/* Erases PEB pnum and writes an EC header + VID header pair (Option A layout: separate
 * pages), simulating what esp_ubinize.py + a UBI-aware flasher would produce on flash. */
inline void format_peb(esp_blockdev_handle_t nand_bdl, uint32_t pnum,
                       uint32_t page_size, uint32_t peb_size,
                       uint32_t image_seq, uint32_t vid_hdr_offset, uint32_t data_offset,
                       uint32_t lnum, uint64_t sqnum,
                       uint8_t copy_flag = 0, uint32_t data_size = 0, uint32_t data_crc = 0)
{
    REQUIRE(nand_bdl->ops->erase(nand_bdl, (uint64_t)pnum * peb_size, peb_size) == ESP_OK);

    std::vector<uint8_t> page(page_size, 0xFF);
    nand_ubi_ec_hdr_t ec {};
    fill_ec_hdr(&ec, image_seq, vid_hdr_offset, data_offset);
    memcpy(page.data(), &ec, sizeof(ec));
    REQUIRE(nand_bdl->ops->write(nand_bdl, page.data(), (uint64_t)pnum * peb_size, page_size) == ESP_OK);

    std::fill(page.begin(), page.end(), 0xFFu);
    nand_ubi_vid_hdr_t vid {};
    fill_vid_hdr(&vid, 0, lnum, sqnum, copy_flag, data_size, data_crc);
    memcpy(page.data(), &vid, sizeof(vid));
    REQUIRE(nand_bdl->ops->write(nand_bdl, page.data(), (uint64_t)pnum * peb_size + vid_hdr_offset, page_size) == ESP_OK);
}

/* Writes LEB data starting at a PEB's data_offset (padded to a page-size multiple,
 * matching what nand_ubi_attach()'s copy_flag verification reads back). */
inline void write_leb_data(esp_blockdev_handle_t nand_bdl, uint32_t pnum, uint32_t peb_size,
                           uint32_t data_offset, uint32_t page_size,
                           const uint8_t *data, uint32_t data_size)
{
    uint32_t write_len = ((data_size + page_size - 1) / page_size) * page_size;
    std::vector<uint8_t> buf(write_len, 0xFF);
    memcpy(buf.data(), data, data_size);
    REQUIRE(nand_bdl->ops->write(nand_bdl, buf.data(), (uint64_t)pnum * peb_size + data_offset, write_len) == ESP_OK);
}

/* Decodes the 2-bit UBI_PEB_* state for pnum out of eba.peb_state (documented packing:
 * 2 bits/PEB, 4 PEBs/byte, LSB-first - see the peb_state field comment in nand_ubi_eba.h).
 * Lets attach() tests distinguish BAD from ERASE_PENDING, which nand_ubi_eba_peb_is_free()
 * alone cannot (it only reports FREE vs. not-FREE). */
inline uint32_t peb_state_of(const nand_ubi_eba_t &eba, uint32_t pnum)
{
    return (eba.peb_state[pnum / 4] >> ((pnum % 4) * 2)) & 0x3u;
}

} // namespace nand_ubi_test
