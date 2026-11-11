/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "spi_nand_flash.h"
#include "nand_linux_mmap_emul.h"
#include "esp_blockdev.h"
#include "esp_nand_blockdev.h"

#include <catch2/catch_test_macros.hpp>

/*
 * Linux host target: CONFIG_NAND_FLASH_GENERIC_CHIP_DETECTION is not available
 * (Kconfig depends on BDL && !IDF_TARGET_LINUX). Generic SPI/ONFI init must not
 * change the mmap emulator path — geometry and chip_source stay on the synthetic
 * database profile.
 */

TEST_CASE("Linux emulator init unchanged when generic detection unavailable", "[linux_emulator]")
{
    nand_file_mmap_emul_config_t conf = {"", 50 * 1024 * 1024, false};
    spi_nand_flash_config_t nand_flash_config = {&conf, 0, SPI_NAND_IO_MODE_SIO, 0};
    esp_blockdev_handle_t bdl = nullptr;

    REQUIRE(nand_flash_get_blockdev(&nand_flash_config, &bdl) == ESP_OK);
    REQUIRE(bdl != nullptr);

    REQUIRE(bdl->geometry.write_size == 2048);
    REQUIRE(bdl->geometry.erase_size == 64 * 2048);

    spi_nand_chip_source_t source = SPI_NAND_CHIP_SOURCE_ONFI;
    REQUIRE(bdl->ops->ioctl(bdl, ESP_BLOCKDEV_CMD_GET_CHIP_SOURCE, &source) == ESP_OK);
    REQUIRE(source == SPI_NAND_CHIP_SOURCE_DATABASE);
    REQUIRE_FALSE(spi_nand_chip_source_is_generic(source));

    bdl->ops->release(bdl);
}
