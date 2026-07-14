/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <catch2/catch_test_macros.hpp>

#include "esp_nand_ubi.h"
#include "nand_ubi_test_helpers.h"

extern "C" {
#include "nand_ubi_priv.h"
}

using namespace nand_ubi_test;

namespace {

constexpr uint32_t kImageSeq = 0xABCD1234u;

struct test_geometry {
    esp_blockdev_handle_t nand_bdl;
    uint32_t page_size;
    uint32_t peb_size;
    uint32_t vid_hdr_offset;
    uint32_t data_offset;
};

test_geometry make_geometry()
{
    test_geometry g {};
    g.nand_bdl = make_test_nand();
    g.page_size = (uint32_t)g.nand_bdl->geometry.read_size;
    g.peb_size = (uint32_t)g.nand_bdl->geometry.erase_size;
    g.vid_hdr_offset = g.page_size;
    g.data_offset = 2u * g.page_size;
    return g;
}

} // namespace

TEST_CASE("attach: format + attach produces a correct EBA table", "[nand_ubi][attach]")
{
    test_geometry g = make_geometry();

    for (uint32_t pnum = 0; pnum < 3; pnum++) {
        format_peb(g.nand_bdl, pnum, g.page_size, g.peb_size, kImageSeq, g.vid_hdr_offset, g.data_offset,
                   /*lnum=*/pnum, /*sqnum=*/pnum + 1);
    }

    nand_ubi_device_t *dev = nullptr;
    REQUIRE(nand_ubi_attach(g.nand_bdl, nullptr, &dev) == ESP_OK);
    REQUIRE(dev != nullptr);

    REQUIRE(dev->peb_size == g.peb_size);
    REQUIRE(dev->page_size == g.page_size);
    REQUIRE(dev->vid_hdr_offset == g.vid_hdr_offset);
    REQUIRE(dev->data_offset == g.data_offset);
    REQUIRE(dev->leb_size == g.peb_size - g.data_offset);
    REQUIRE(dev->image_seq == kImageSeq);
    REQUIRE(dev->leb_count == 3);
    REQUIRE(dev->global_sqnum == 3);

    for (uint32_t lnum = 0; lnum < 3; lnum++) {
        REQUIRE(nand_ubi_eba_get_pnum(&dev->eba, lnum) == (int32_t)lnum);
        REQUIRE(peb_state_of(dev->eba, lnum) == UBI_PEB_USED);
    }

    REQUIRE(nand_ubi_detach(dev) == ESP_OK);
    g.nand_bdl->ops->release(g.nand_bdl);
}

TEST_CASE("attach: blank/unformatted chip attaches with zero LEBs", "[nand_ubi][attach]")
{
    test_geometry g = make_geometry();

    nand_ubi_device_t *dev = nullptr;
    REQUIRE(nand_ubi_attach(g.nand_bdl, nullptr, &dev) == ESP_OK);
    REQUIRE(dev != nullptr);
    REQUIRE(dev->leb_count == 0);
    REQUIRE(dev->image_seq == 0);
    REQUIRE(dev->vid_hdr_offset == g.page_size);
    REQUIRE(dev->data_offset == 2u * g.page_size);

    REQUIRE(nand_ubi_detach(dev) == ESP_OK);
    g.nand_bdl->ops->release(g.nand_bdl);
}

TEST_CASE("attach: factory bad blocks are excluded from the EBA", "[nand_ubi][attach]")
{
    test_geometry g = make_geometry();

    uint32_t bad_pnum = 5;
    REQUIRE(g.nand_bdl->ops->ioctl(g.nand_bdl, ESP_BLOCKDEV_CMD_MARK_BAD_BLOCK, &bad_pnum) == ESP_OK);

    /* Format three good PEBs around the bad one; LEB numbering must stay contiguous
     * and none of them may resolve to the bad physical block. */
    format_peb(g.nand_bdl, 0, g.page_size, g.peb_size, kImageSeq, g.vid_hdr_offset, g.data_offset, 0, 1);
    format_peb(g.nand_bdl, 1, g.page_size, g.peb_size, kImageSeq, g.vid_hdr_offset, g.data_offset, 1, 2);
    format_peb(g.nand_bdl, 6, g.page_size, g.peb_size, kImageSeq, g.vid_hdr_offset, g.data_offset, 2, 3);

    nand_ubi_device_t *dev = nullptr;
    REQUIRE(nand_ubi_attach(g.nand_bdl, nullptr, &dev) == ESP_OK);

    REQUIRE(dev->leb_count == 3);
    REQUIRE(peb_state_of(dev->eba, bad_pnum) == UBI_PEB_BAD);
    for (uint32_t lnum = 0; lnum < 3; lnum++) {
        int32_t pnum = nand_ubi_eba_get_pnum(&dev->eba, lnum);
        REQUIRE(pnum != (int32_t)bad_pnum);
    }
    REQUIRE(nand_ubi_eba_get_pnum(&dev->eba, 0) == 0);
    REQUIRE(nand_ubi_eba_get_pnum(&dev->eba, 1) == 1);
    REQUIRE(nand_ubi_eba_get_pnum(&dev->eba, 2) == 6);

    REQUIRE(nand_ubi_detach(dev) == ESP_OK);
    g.nand_bdl->ops->release(g.nand_bdl);
}

TEST_CASE("attach: corrupt EC header is scheduled for erase, not treated as free", "[nand_ubi][attach]")
{
    test_geometry g = make_geometry();

    REQUIRE(g.nand_bdl->ops->erase(g.nand_bdl, 0, g.peb_size) == ESP_OK);
    std::vector<uint8_t> garbage(g.page_size, 0xA5);
    REQUIRE(g.nand_bdl->ops->write(g.nand_bdl, garbage.data(), 0, g.page_size) == ESP_OK);

    nand_ubi_device_t *dev = nullptr;
    REQUIRE(nand_ubi_attach(g.nand_bdl, nullptr, &dev) == ESP_OK);
    REQUIRE(dev->leb_count == 0);
    REQUIRE(peb_state_of(dev->eba, 0) == UBI_PEB_ERASE_PENDING);

    REQUIRE(nand_ubi_detach(dev) == ESP_OK);
    g.nand_bdl->ops->release(g.nand_bdl);
}

TEST_CASE("attach: duplicate PEBs resolved by higher sqnum, loser scheduled for erase", "[nand_ubi][attach]")
{
    test_geometry g = make_geometry();

    /* Simulated interrupted power-cut mid-write: two PEBs both claim lnum=0. */
    format_peb(g.nand_bdl, 0, g.page_size, g.peb_size, kImageSeq, g.vid_hdr_offset, g.data_offset,
               /*lnum=*/0, /*sqnum=*/5);
    format_peb(g.nand_bdl, 1, g.page_size, g.peb_size, kImageSeq, g.vid_hdr_offset, g.data_offset,
               /*lnum=*/0, /*sqnum=*/9);

    nand_ubi_device_t *dev = nullptr;
    REQUIRE(nand_ubi_attach(g.nand_bdl, nullptr, &dev) == ESP_OK);

    REQUIRE(dev->leb_count == 1);
    REQUIRE(dev->global_sqnum == 9);
    REQUIRE(nand_ubi_eba_get_pnum(&dev->eba, 0) == 1);
    REQUIRE(peb_state_of(dev->eba, 1) == UBI_PEB_USED);
    REQUIRE(peb_state_of(dev->eba, 0) == UBI_PEB_ERASE_PENDING);

    REQUIRE(nand_ubi_detach(dev) == ESP_OK);
    g.nand_bdl->ops->release(g.nand_bdl);
}

TEST_CASE("attach: copy_flag with matching data_crc accepts the newer copy", "[nand_ubi][attach]")
{
    test_geometry g = make_geometry();

    std::vector<uint8_t> leb_data(64, 0x42);
    uint32_t data_crc = nand_ubi_crc32(leb_data.data(), (uint32_t)leb_data.size());

    format_peb(g.nand_bdl, 0, g.page_size, g.peb_size, kImageSeq, g.vid_hdr_offset, g.data_offset,
               /*lnum=*/0, /*sqnum=*/5);
    format_peb(g.nand_bdl, 1, g.page_size, g.peb_size, kImageSeq, g.vid_hdr_offset, g.data_offset,
               /*lnum=*/0, /*sqnum=*/9, /*copy_flag=*/1, (uint32_t)leb_data.size(), data_crc);
    write_leb_data(g.nand_bdl, 1, g.peb_size, g.data_offset, g.page_size, leb_data.data(), (uint32_t)leb_data.size());

    nand_ubi_device_t *dev = nullptr;
    REQUIRE(nand_ubi_attach(g.nand_bdl, nullptr, &dev) == ESP_OK);

    REQUIRE(nand_ubi_eba_get_pnum(&dev->eba, 0) == 1);
    REQUIRE(peb_state_of(dev->eba, 1) == UBI_PEB_USED);
    REQUIRE(peb_state_of(dev->eba, 0) == UBI_PEB_ERASE_PENDING);

    REQUIRE(nand_ubi_detach(dev) == ESP_OK);
    g.nand_bdl->ops->release(g.nand_bdl);
}

TEST_CASE("attach: copy_flag with mismatched data_crc falls back to the older copy", "[nand_ubi][attach]")
{
    test_geometry g = make_geometry();

    std::vector<uint8_t> leb_data(64, 0x42);
    uint32_t bogus_crc = nand_ubi_crc32(leb_data.data(), (uint32_t)leb_data.size()) ^ 0xFFFFFFFFu;

    format_peb(g.nand_bdl, 0, g.page_size, g.peb_size, kImageSeq, g.vid_hdr_offset, g.data_offset,
               /*lnum=*/0, /*sqnum=*/5);
    format_peb(g.nand_bdl, 1, g.page_size, g.peb_size, kImageSeq, g.vid_hdr_offset, g.data_offset,
               /*lnum=*/0, /*sqnum=*/9, /*copy_flag=*/1, (uint32_t)leb_data.size(), bogus_crc);
    write_leb_data(g.nand_bdl, 1, g.peb_size, g.data_offset, g.page_size, leb_data.data(), (uint32_t)leb_data.size());

    nand_ubi_device_t *dev = nullptr;
    REQUIRE(nand_ubi_attach(g.nand_bdl, nullptr, &dev) == ESP_OK);

    /* pnum 1's data_crc doesn't match -> pnum 0 (the older, valid copy) must win. */
    REQUIRE(nand_ubi_eba_get_pnum(&dev->eba, 0) == 0);
    REQUIRE(peb_state_of(dev->eba, 0) == UBI_PEB_USED);
    REQUIRE(peb_state_of(dev->eba, 1) == UBI_PEB_ERASE_PENDING);

    REQUIRE(nand_ubi_detach(dev) == ESP_OK);
    g.nand_bdl->ops->release(g.nand_bdl);
}

TEST_CASE("attach: rejects null arguments", "[nand_ubi][attach]")
{
    test_geometry g = make_geometry();
    nand_ubi_device_t *dev = nullptr;

    REQUIRE(nand_ubi_attach(nullptr, nullptr, &dev) == ESP_ERR_INVALID_ARG);
    REQUIRE(nand_ubi_attach(g.nand_bdl, nullptr, nullptr) == ESP_ERR_INVALID_ARG);
    REQUIRE(nand_ubi_detach(nullptr) == ESP_ERR_INVALID_ARG);

    g.nand_bdl->ops->release(g.nand_bdl);
}
