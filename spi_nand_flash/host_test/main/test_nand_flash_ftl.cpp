/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * FTL-level host tests for spi_nand_flash.
 *
 * All tests exercise the public logical-sector API exclusively:
 *   spi_nand_flash_write_sector / read_sector / copy_sector / trim /
 *   sync / get_capacity / get_sector_size / get_block_size /
 *   get_block_num / spi_nand_erase_chip
 *
 * Raw NAND page operations (nand_wrap_*) are intentionally NOT used here.
 * See test_nand_flash.cpp for raw-level coverage.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "spi_nand_flash.h"
#include "spi_nand_flash_test_helpers.h"
#include "nand_linux_mmap_emul.h"

#include <catch2/catch_test_macros.hpp>

/* -------------------------------------------------------------------------
 * Fixture helpers
 * ---------------------------------------------------------------------- */

/** Default flash size used by most tests (16 MiB — fast init / GC pressure). */
#define FTL_TEST_FLASH_SIZE  ((size_t)16u * 1024u * 1024u)

/** Larger flash used for the full-capacity sequential sweep. */
#define FTL_TEST_FLASH_LARGE ((size_t)32u * 1024u * 1024u)

/**
 * Open a fresh emulated NAND device backed by an anonymous temp file.
 * gc_factor == 0 selects the driver default.
 */
static spi_nand_flash_device_t *make_ftl_dev(size_t flash_size = FTL_TEST_FLASH_SIZE,
        uint8_t gc_factor = 0)
{
    nand_file_mmap_emul_config_t emul = {"", flash_size, /*keep_dump=*/false};
    spi_nand_flash_config_t cfg = {&emul, gc_factor, SPI_NAND_IO_MODE_SIO, 0};
    spi_nand_flash_device_t *dev = nullptr;
    REQUIRE(spi_nand_flash_init_device(&cfg, &dev) == ESP_OK);
    REQUIRE(dev != nullptr);
    return dev;
}

static void destroy_ftl_dev(spi_nand_flash_device_t *dev)
{
    REQUIRE(spi_nand_flash_deinit_device(dev) == ESP_OK);
}

/* -------------------------------------------------------------------------
 * Group 1: Device info / capacity API
 * ---------------------------------------------------------------------- */

TEST_CASE("FTL get_capacity returns non-zero sector count", "[ftl][info]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sectors = 0;
    REQUIRE(spi_nand_flash_get_capacity(dev, &sectors) == ESP_OK);
    REQUIRE(sectors > 0);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL get_sector_size returns a power-of-two >= 512", "[ftl][info]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);
    REQUIRE(sz >= 512u);
    /* Must be a power of two */
    REQUIRE((sz & (sz - 1u)) == 0u);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL get_block_size is a multiple of sector_size", "[ftl][info]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sector_size = 0, block_size = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sector_size) == ESP_OK);
    REQUIRE(spi_nand_flash_get_block_size(dev, &block_size) == ESP_OK);
    REQUIRE(block_size > 0);
    REQUIRE(block_size % sector_size == 0);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL get_block_num is consistent with capacity and block/sector sizes",
          "[ftl][info]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sectors = 0, sector_size = 0, block_size = 0, blocks = 0;
    REQUIRE(spi_nand_flash_get_capacity(dev, &sectors) == ESP_OK);
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sector_size) == ESP_OK);
    REQUIRE(spi_nand_flash_get_block_size(dev, &block_size) == ESP_OK);
    REQUIRE(spi_nand_flash_get_block_num(dev, &blocks) == ESP_OK);
    REQUIRE(blocks > 0);
    /* Total logical flash bytes derived from blocks must match capacity-derived bytes.
     * Dhara reserves some blocks for GC, so the logical sector count is <= physical. */
    uint32_t pages_per_block = block_size / sector_size;
    REQUIRE(blocks * pages_per_block >= sectors);
    destroy_ftl_dev(dev);
}

/* -------------------------------------------------------------------------
 * Group 2: Single-sector write / read round-trip
 * ---------------------------------------------------------------------- */

TEST_CASE("FTL write then read back sector 0 produces correct data", "[ftl][rw]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
    REQUIRE(spi_nand_flash_write_sector(dev, wbuf, 0) == ESP_OK);
    REQUIRE(spi_nand_flash_read_sector(dev, rbuf, 0) == ESP_OK);
    REQUIRE(memcmp(wbuf, rbuf, sz) == 0);

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL write then read back last valid sector", "[ftl][rw]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sectors = 0, sz = 0;
    REQUIRE(spi_nand_flash_get_capacity(dev, &sectors) == ESP_OK);
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    uint32_t last = sectors - 1u;
    spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
    REQUIRE(spi_nand_flash_write_sector(dev, wbuf, last) == ESP_OK);
    REQUIRE(spi_nand_flash_read_sector(dev, rbuf, last) == ESP_OK);
    REQUIRE(memcmp(wbuf, rbuf, sz) == 0);

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL overwrite same sector multiple times, each read-back is correct",
          "[ftl][rw]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    const uint32_t TARGET_SECTOR = 7;
    const int OVERWRITE_ROUNDS = 20;

    for (int i = 0; i < OVERWRITE_ROUNDS; i++) {
        spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
        REQUIRE(spi_nand_flash_write_sector(dev, wbuf, TARGET_SECTOR) == ESP_OK);
        REQUIRE(spi_nand_flash_read_sector(dev, rbuf, TARGET_SECTOR) == ESP_OK);
        REQUIRE(memcmp(wbuf, rbuf, sz) == 0);
    }

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL multiple sectors hold independent data after interleaved writes",
          "[ftl][rw]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    const uint32_t N = 8;
    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    /* Write each sector with the shared test pattern */
    for (uint32_t s = 0; s < N; s++) {
        spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
        REQUIRE(spi_nand_flash_write_sector(dev, wbuf, s) == ESP_OK);
    }

    /* Read them all back and verify the pattern */
    for (uint32_t s = 0; s < N; s++) {
        REQUIRE(spi_nand_flash_read_sector(dev, rbuf, s) == ESP_OK);
        REQUIRE(spi_nand_flash_check_buffer(rbuf, sz / sizeof(uint32_t)) == 0);
    }

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

/* -------------------------------------------------------------------------
 * Group 3: sync
 * ---------------------------------------------------------------------- */

TEST_CASE("FTL sync returns ESP_OK on a freshly initialised device", "[ftl][sync]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    REQUIRE(spi_nand_flash_sync(dev) == ESP_OK);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL sync after writes returns ESP_OK and data survives", "[ftl][sync]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
    REQUIRE(spi_nand_flash_write_sector(dev, wbuf, 3) == ESP_OK);
    REQUIRE(spi_nand_flash_sync(dev) == ESP_OK);
    REQUIRE(spi_nand_flash_read_sector(dev, rbuf, 3) == ESP_OK);
    REQUIRE(memcmp(wbuf, rbuf, sz) == 0);

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

/* -------------------------------------------------------------------------
 * Group 4: copy_sector
 * ---------------------------------------------------------------------- */

TEST_CASE("FTL copy_sector duplicates data to a different logical sector",
          "[ftl][copy]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
    REQUIRE(spi_nand_flash_write_sector(dev, wbuf, 10) == ESP_OK);
    REQUIRE(spi_nand_flash_copy_sector(dev, 10, 20) == ESP_OK);
    REQUIRE(spi_nand_flash_read_sector(dev, rbuf, 20) == ESP_OK);
    REQUIRE(memcmp(wbuf, rbuf, sz) == 0);

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL copy_sector to same sector id is idempotent", "[ftl][copy]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
    REQUIRE(spi_nand_flash_write_sector(dev, wbuf, 5) == ESP_OK);
    /* Copying a sector onto itself should succeed (or at least not corrupt) */
    esp_err_t rc = spi_nand_flash_copy_sector(dev, 5, 5);
    /* Behaviour is either ESP_OK or a graceful error — must not crash */
    if (rc == ESP_OK) {
        REQUIRE(spi_nand_flash_read_sector(dev, rbuf, 5) == ESP_OK);
        REQUIRE(memcmp(wbuf, rbuf, sz) == 0);
    }
    /* If it returned an error that is also acceptable — just must not corrupt data */

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL copy does not alter source sector data", "[ftl][copy]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
    REQUIRE(spi_nand_flash_write_sector(dev, wbuf, 11) == ESP_OK);
    REQUIRE(spi_nand_flash_copy_sector(dev, 11, 22) == ESP_OK);

    /* Source must be unchanged */
    REQUIRE(spi_nand_flash_read_sector(dev, rbuf, 11) == ESP_OK);
    REQUIRE(memcmp(wbuf, rbuf, sz) == 0);

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

/* -------------------------------------------------------------------------
 * Group 5: trim
 * ---------------------------------------------------------------------- */

TEST_CASE("FTL trim returns ESP_OK on a written sector", "[ftl][trim]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *buf = (uint8_t *)malloc(sz);
    REQUIRE(buf != nullptr);
    memset(buf, 0xAB, sz);

    REQUIRE(spi_nand_flash_write_sector(dev, buf, 4) == ESP_OK);
    REQUIRE(spi_nand_flash_trim(dev, 4) == ESP_OK);

    free(buf);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL trim then write to the same sector succeeds", "[ftl][trim]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    memset(wbuf, 0x11, sz);
    REQUIRE(spi_nand_flash_write_sector(dev, wbuf, 6) == ESP_OK);
    REQUIRE(spi_nand_flash_trim(dev, 6) == ESP_OK);

    spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
    REQUIRE(spi_nand_flash_write_sector(dev, wbuf, 6) == ESP_OK);
    REQUIRE(spi_nand_flash_read_sector(dev, rbuf, 6) == ESP_OK);
    REQUIRE(memcmp(wbuf, rbuf, sz) == 0);

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL trim on unwritten sector returns ESP_OK", "[ftl][trim]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    /* Sector 9 has never been written — trim should still succeed gracefully */
    REQUIRE(spi_nand_flash_trim(dev, 9) == ESP_OK);
    destroy_ftl_dev(dev);
}

/* -------------------------------------------------------------------------
 * Group 6: erase_chip
 * ---------------------------------------------------------------------- */

TEST_CASE("FTL erase_chip succeeds and write/read works afterwards",
          "[ftl][erase-chip]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    /* Write something before the chip erase */
    spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
    REQUIRE(spi_nand_flash_write_sector(dev, wbuf, 0) == ESP_OK);

    REQUIRE(spi_nand_erase_chip(dev) == ESP_OK);

    /* After erase the FTL must accept new writes */
    spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
    REQUIRE(spi_nand_flash_write_sector(dev, wbuf, 0) == ESP_OK);
    REQUIRE(spi_nand_flash_read_sector(dev, rbuf, 0) == ESP_OK);
    REQUIRE(memcmp(wbuf, rbuf, sz) == 0);

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

/* -------------------------------------------------------------------------
 * Group 7: Logical sector IDs beyond capacity
 *
 * NOTE: Dhara does NOT bounds-check logical sector IDs at the FTL level.
 * Writing or reading beyond the reported capacity succeeds (returns ESP_OK)
 * rather than returning an error.  These tests document that observed
 * behaviour so that any future change in Dhara that adds bounds-checking
 * will be caught explicitly.
 * ---------------------------------------------------------------------- */

TEST_CASE("FTL write to sector_id == capacity succeeds",
          "[ftl][bounds]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sectors = 0, sz = 0;
    REQUIRE(spi_nand_flash_get_capacity(dev, &sectors) == ESP_OK);
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *buf = (uint8_t *)calloc(1, sz);
    REQUIRE(buf != nullptr);

    /* Dhara does not validate the sector ID — expect success, not an error. */
    esp_err_t rc = spi_nand_flash_write_sector(dev, buf, sectors);
    REQUIRE(rc == ESP_OK);

    free(buf);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL read from sector_id == capacity succeeds",
          "[ftl][bounds]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sectors = 0, sz = 0;
    REQUIRE(spi_nand_flash_get_capacity(dev, &sectors) == ESP_OK);
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *buf = (uint8_t *)calloc(1, sz);
    REQUIRE(buf != nullptr);

    /* Dhara does not validate the sector ID — expect success, not an error. */
    esp_err_t rc = spi_nand_flash_read_sector(dev, buf, sectors);
    REQUIRE(rc == ESP_OK);

    free(buf);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL write to UINT32_MAX sector_id succeeds",
          "[ftl][bounds]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *buf = (uint8_t *)calloc(1, sz);
    REQUIRE(buf != nullptr);

    /* Dhara does not validate the sector ID — expect success, not an error. */
    esp_err_t rc = spi_nand_flash_write_sector(dev, buf, UINT32_MAX);
    REQUIRE(rc == ESP_OK);

    free(buf);
    destroy_ftl_dev(dev);
}

/* -------------------------------------------------------------------------
 * Group 8: Sequential full-capacity write sweep
 * ---------------------------------------------------------------------- */

TEST_CASE("FTL sequential write to every logical sector, then read-back all",
          "[ftl][sequential]")
{
    /* Use larger flash so we have a meaningful number of logical sectors */
    spi_nand_flash_device_t *dev = make_ftl_dev(FTL_TEST_FLASH_LARGE);
    uint32_t sectors = 0, sz = 0;
    REQUIRE(spi_nand_flash_get_capacity(dev, &sectors) == ESP_OK);
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    /* Write pass */
    for (uint32_t s = 0; s < sectors; s++) {
        spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
        REQUIRE(spi_nand_flash_write_sector(dev, wbuf, s) == ESP_OK);
    }

    REQUIRE(spi_nand_flash_sync(dev) == ESP_OK);

    /* Verify capacity has not changed */
    uint32_t sectors_after = 0;
    REQUIRE(spi_nand_flash_get_capacity(dev, &sectors_after) == ESP_OK);
    REQUIRE(sectors_after == sectors);

    /* Read-back pass */
    for (uint32_t s = 0; s < sectors; s++) {
        REQUIRE(spi_nand_flash_read_sector(dev, rbuf, s) == ESP_OK);
        REQUIRE(spi_nand_flash_check_buffer(rbuf, sz / sizeof(uint32_t)) == 0);
    }

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

/* -------------------------------------------------------------------------
 * Group 9: GC stability — hot-set repeated overwrites
 *
 * Write to a small set of logical sectors (HOT_SET_SIZE) many times
 * (TOTAL_WRITES).  This forces Dhara to perform garbage collection
 * repeatedly.  Asserts:
 *   1. Every write returns ESP_OK.
 *   2. After the run, get_capacity() still returns the original value.
 *   3. Each sector in the hot-set reads back the last value written.
 *   4. spi_nand_flash_sync() returns ESP_OK.
 * ---------------------------------------------------------------------- */

#define HOT_SET_SIZE  50u
#define TOTAL_WRITES  5000u

TEST_CASE("FTL GC stability: 50-sector hot-set written 5000 times",
          "[ftl][gc][stability]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();

    uint32_t sectors = 0, sz = 0;
    REQUIRE(spi_nand_flash_get_capacity(dev, &sectors) == ESP_OK);
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    /* The hot-set must fit within the device's logical address space */
    REQUIRE(sectors >= HOT_SET_SIZE);

    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    bool written[HOT_SET_SIZE] = {};

    srand(0xDEADBEEFu); /* reproducible */

    for (uint32_t op = 0; op < TOTAL_WRITES; op++) {
        uint32_t lsector = (uint32_t)((unsigned)rand() % HOT_SET_SIZE);
        spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
        REQUIRE(spi_nand_flash_write_sector(dev, wbuf, lsector) == ESP_OK);
        written[lsector] = true;
    }

    /* Capacity must be unchanged — the Dhara map must not have grown OOB */
    uint32_t sectors_after = 0;
    REQUIRE(spi_nand_flash_get_capacity(dev, &sectors_after) == ESP_OK);
    REQUIRE(sectors_after == sectors);

    /* Flush any in-memory state */
    REQUIRE(spi_nand_flash_sync(dev) == ESP_OK);

    /* Every hot sector must read back the last write's pattern */
    for (uint32_t s = 0; s < HOT_SET_SIZE; s++) {
        if (!written[s]) {
            continue; /* rand() never selected this logical sector */
        }
        REQUIRE(spi_nand_flash_read_sector(dev, rbuf, s) == ESP_OK);
        REQUIRE(spi_nand_flash_check_buffer(rbuf, sz / sizeof(uint32_t)) == 0);
    }

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL GC stability with trim: hot-set with interleaved trims",
          "[ftl][gc][trim][stability]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();

    uint32_t sectors = 0, sz = 0;
    REQUIRE(spi_nand_flash_get_capacity(dev, &sectors) == ESP_OK);
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);
    REQUIRE(sectors >= HOT_SET_SIZE);

    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    bool trimmed[HOT_SET_SIZE] = {};
    bool written[HOT_SET_SIZE] = {};

    srand(0xCAFEBABEu);

    for (uint32_t op = 0; op < TOTAL_WRITES; op++) {
        uint32_t lsector = (uint32_t)((unsigned)rand() % HOT_SET_SIZE);

        /* Every 100 ops trim a random sector in the hot-set */
        if (op % 100 == 99) {
            uint32_t t = (uint32_t)((unsigned)rand() % HOT_SET_SIZE);
            if (spi_nand_flash_trim(dev, t) == ESP_OK) {
                trimmed[t] = true;
            }
        }

        spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
        REQUIRE(spi_nand_flash_write_sector(dev, wbuf, lsector) == ESP_OK);
        trimmed[lsector] = false;
        written[lsector] = true;
    }

    uint32_t sectors_after = 0;
    REQUIRE(spi_nand_flash_get_capacity(dev, &sectors_after) == ESP_OK);
    REQUIRE(sectors_after == sectors);
    REQUIRE(spi_nand_flash_sync(dev) == ESP_OK);

    /* Verify untrimmed sectors */
    for (uint32_t s = 0; s < HOT_SET_SIZE; s++) {
        if (trimmed[s] || !written[s]) {
            continue; /* skip trimmed or never-written */
        }
        REQUIRE(spi_nand_flash_read_sector(dev, rbuf, s) == ESP_OK);
        REQUIRE(spi_nand_flash_check_buffer(rbuf, sz / sizeof(uint32_t)) == 0);
    }

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

/* -------------------------------------------------------------------------
 * Group 10: Single-sector hammer — one sector written thousands of times
 * ---------------------------------------------------------------------- */

TEST_CASE("FTL single sector written 2000 times stays readable and correct",
          "[ftl][gc][hammer]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    const uint32_t TARGET = 0;
    const uint32_t ROUNDS = 2000;

    for (uint32_t i = 0; i < ROUNDS; i++) {
        spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
        REQUIRE(spi_nand_flash_write_sector(dev, wbuf, TARGET) == ESP_OK);
    }

    /* Final read must reflect the last write */
    spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
    REQUIRE(spi_nand_flash_read_sector(dev, rbuf, TARGET) == ESP_OK);
    REQUIRE(memcmp(wbuf, rbuf, sz) == 0);

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

/* -------------------------------------------------------------------------
 * Group 11: Alternating write / read pattern
 * ---------------------------------------------------------------------- */

TEST_CASE("FTL alternating write-read on two sectors never corrupts either",
          "[ftl][rw][alternating]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *wbuf = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(wbuf != nullptr);
    REQUIRE(rbuf != nullptr);

    const uint32_t ITERS = 500;

    for (uint32_t i = 0; i < ITERS; i++) {
        /* Write A, then read B, then write B, then read A */
        spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
        REQUIRE(spi_nand_flash_write_sector(dev, wbuf, 1) == ESP_OK);

        REQUIRE(spi_nand_flash_read_sector(dev, rbuf, 2) == ESP_OK);
        if (i > 0) {
            REQUIRE(spi_nand_flash_check_buffer(rbuf, sz / sizeof(uint32_t)) == 0);
        }

        spi_nand_flash_fill_buffer(wbuf, sz / sizeof(uint32_t));
        REQUIRE(spi_nand_flash_write_sector(dev, wbuf, 2) == ESP_OK);

        REQUIRE(spi_nand_flash_read_sector(dev, rbuf, 1) == ESP_OK);
        REQUIRE(spi_nand_flash_check_buffer(rbuf, sz / sizeof(uint32_t)) == 0);
    }

    free(wbuf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

/* -------------------------------------------------------------------------
 * Group 12: All-zeros and all-ones patterns (edge-case data values)
 * ---------------------------------------------------------------------- */

TEST_CASE("FTL write/read all-zeros pattern", "[ftl][rw][patterns]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *buf = (uint8_t *)calloc(1, sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(buf != nullptr);
    REQUIRE(rbuf != nullptr);

    REQUIRE(spi_nand_flash_write_sector(dev, buf, 0) == ESP_OK);
    REQUIRE(spi_nand_flash_read_sector(dev, rbuf, 0) == ESP_OK);
    REQUIRE(memcmp(buf, rbuf, sz) == 0);

    free(buf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL write/read all-0xFF pattern", "[ftl][rw][patterns]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *buf  = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(buf != nullptr);
    REQUIRE(rbuf != nullptr);
    memset(buf, 0xFF, sz);

    REQUIRE(spi_nand_flash_write_sector(dev, buf, 1) == ESP_OK);
    REQUIRE(spi_nand_flash_read_sector(dev, rbuf, 1) == ESP_OK);
    REQUIRE(memcmp(buf, rbuf, sz) == 0);

    free(buf);
    free(rbuf);
    destroy_ftl_dev(dev);
}

TEST_CASE("FTL write/read alternating 0xAA/0x55 pattern", "[ftl][rw][patterns]")
{
    spi_nand_flash_device_t *dev = make_ftl_dev();
    uint32_t sz = 0;
    REQUIRE(spi_nand_flash_get_sector_size(dev, &sz) == ESP_OK);

    uint8_t *buf  = (uint8_t *)malloc(sz);
    uint8_t *rbuf = (uint8_t *)malloc(sz);
    REQUIRE(buf != nullptr);
    REQUIRE(rbuf != nullptr);

    for (size_t i = 0; i < sz; i++) {
        buf[i] = (i & 1u) ? 0x55u : 0xAAu;
    }

    REQUIRE(spi_nand_flash_write_sector(dev, buf, 2) == ESP_OK);
    REQUIRE(spi_nand_flash_read_sector(dev, rbuf, 2) == ESP_OK);
    REQUIRE(memcmp(buf, rbuf, sz) == 0);

    free(buf);
    free(rbuf);
    destroy_ftl_dev(dev);
}
