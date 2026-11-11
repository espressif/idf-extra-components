/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include <inttypes.h>
#include "unity.h"
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "spi_nand_flash.h"
#include "esp_blockdev.h"
#include "esp_nand_blockdev.h"
#include "test_spi_nand_common.h"

#if CONFIG_NAND_FLASH_GENERIC_CHIP_DETECTION

static void setup_flash_bdl(spi_device_handle_t *spi_out, esp_blockdev_handle_t *bdl_out, uint8_t flags)
{
    spi_device_handle_t spi;
    spi_nand_test_setup_chip(&spi, flags);

    spi_nand_flash_config_t cfg = {
        .device_handle = spi,
        .flags = flags,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t bdl = NULL;
    TEST_ESP_OK(nand_flash_get_blockdev(&cfg, &bdl));
    TEST_ASSERT_NOT_NULL(bdl);
    *spi_out = spi;
    *bdl_out = bdl;
}

static void teardown_flash_bdl(spi_device_handle_t spi, esp_blockdev_handle_t bdl)
{
    (void)spi;
    bdl->ops->release(bdl);
    spi_nand_flash_test_teardown();
}

TEST_CASE("generic detect: Flash BDL init, read-only, probe report", "[spi_nand_flash][generic]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t bdl;
    setup_flash_bdl(&spi, &bdl, SPI_DEVICE_HALFDUPLEX);

#if CONFIG_NAND_FLASH_GENERIC_WRITE_ERASE_ENABLE
    TEST_ASSERT_FALSE(bdl->device_flags.read_only);
#else
    TEST_ASSERT_TRUE(bdl->device_flags.read_only);
#endif /* CONFIG_NAND_FLASH_GENERIC_WRITE_ERASE_ENABLE */

    spi_nand_chip_source_t source = SPI_NAND_CHIP_SOURCE_DATABASE;
    TEST_ESP_OK(bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_CHIP_SOURCE, &source));
    TEST_ASSERT_TRUE(spi_nand_chip_source_is_generic(source));
    TEST_ASSERT_TRUE(source == SPI_NAND_CHIP_SOURCE_ONFI || source == SPI_NAND_CHIP_SOURCE_KCONFIG);

    spi_nand_generic_probe_report_t report;
    memset(&report, 0, sizeof(report));
    TEST_ESP_OK(bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_GENERIC_PROBE_REPORT, &report));
    TEST_ASSERT_TRUE(report.page0_read_ok);
    TEST_ASSERT_TRUE(report.param_page_valid);
    TEST_ASSERT_EQUAL(CONFIG_NAND_FLASH_GENERIC_NUM_PLANES, report.num_planes);
#if CONFIG_NAND_FLASH_GENERIC_WRITE_ERASE_ENABLE
    TEST_ASSERT_TRUE(report.write_erase_enabled);
    TEST_ASSERT_TRUE(report.on_die_ecc_enabled);
#else
    TEST_ASSERT_FALSE(report.write_erase_enabled);
#endif /* CONFIG_NAND_FLASH_GENERIC_WRITE_ERASE_ENABLE */

    uint32_t page_size = bdl->geometry.read_size;
    uint8_t *buf = heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ESP_OK(bdl->ops->read(bdl, buf, page_size, 0, page_size));
    free(buf);

    teardown_flash_bdl(spi, bdl);
}

TEST_CASE("generic detect: full-duplex SIO parameter page", "[spi_nand_flash][generic]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t bdl;
    /* flags 0: full-duplex. The parameter-page read then uses the temporary DMA bounce. */
    setup_flash_bdl(&spi, &bdl, 0);

    spi_nand_chip_source_t source = SPI_NAND_CHIP_SOURCE_DATABASE;
    TEST_ESP_OK(bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_CHIP_SOURCE, &source));
    TEST_ASSERT_EQUAL(SPI_NAND_CHIP_SOURCE_ONFI, source);

    spi_nand_generic_probe_report_t report;
    memset(&report, 0, sizeof(report));
    TEST_ESP_OK(bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_GENERIC_PROBE_REPORT, &report));
    TEST_ASSERT_TRUE(report.page0_read_ok);
    TEST_ASSERT_TRUE(report.param_page_valid);

    teardown_flash_bdl(spi, bdl);
}

TEST_CASE("generic detect: wear-leveling and init_with_layers unsupported", "[spi_nand_flash][generic]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t bdl;
    setup_flash_bdl(&spi, &bdl, SPI_DEVICE_HALFDUPLEX);

    esp_blockdev_handle_t wl = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, spi_nand_flash_wl_get_blockdev(bdl, &wl));
    TEST_ASSERT_NULL(wl);

    teardown_flash_bdl(spi, bdl);

    spi_nand_test_setup_chip(&spi, SPI_DEVICE_HALFDUPLEX);
    spi_nand_flash_config_t cfg = {
        .device_handle = spi,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .io_mode = SPI_NAND_IO_MODE_SIO,
    };
    esp_blockdev_handle_t wl_bdl = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, spi_nand_flash_init_with_layers(&cfg, &wl_bdl));
    TEST_ASSERT_NULL(wl_bdl);
    spi_nand_flash_test_teardown();
}

#if !CONFIG_NAND_FLASH_GENERIC_WRITE_ERASE_ENABLE
TEST_CASE("generic detect: write and erase rejected when read-only", "[spi_nand_flash][generic]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t bdl;
    setup_flash_bdl(&spi, &bdl, SPI_DEVICE_HALFDUPLEX);

    uint32_t page_size = bdl->geometry.write_size;
    uint8_t *buf = heap_caps_malloc(page_size, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(buf);
    memset(buf, 0xA5, page_size);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, bdl->ops->write(bdl, buf, 0, page_size));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, bdl->ops->erase(bdl, 0, bdl->geometry.erase_size));
    free(buf);

    teardown_flash_bdl(spi, bdl);
}
#endif /* !CONFIG_NAND_FLASH_GENERIC_WRITE_ERASE_ENABLE */

TEST_CASE("generic detect: ECC status ioctl reports unknown", "[spi_nand_flash][generic]")
{
    spi_device_handle_t spi;
    esp_blockdev_handle_t bdl;
    setup_flash_bdl(&spi, &bdl, SPI_DEVICE_HALFDUPLEX);

    esp_blockdev_cmd_arg_ecc_status_t ecc_cmd = { .page_num = 0 };
    TEST_ESP_OK(bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_PAGE_ECC_STATUS, &ecc_cmd));
    TEST_ASSERT_EQUAL(NAND_ECC_UNKNOWN, ecc_cmd.ecc_status);

    esp_blockdev_cmd_arg_ecc_stats_t ecc_stats;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_ECC_STATS, &ecc_stats));

    teardown_flash_bdl(spi, bdl);
}

#endif /* CONFIG_NAND_FLASH_GENERIC_CHIP_DETECTION */
