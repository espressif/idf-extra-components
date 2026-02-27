# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize
import glob
from pathlib import Path


@pytest.mark.spi_nand_flash
@pytest.mark.skipif(
    not bool(glob.glob(f'{Path(__file__).parent.absolute()}/build*/')),
    reason="Skip the idf version that not build"
)
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_nand_flash_example(dut) -> None:
    dut.expect_exact("Opening file")
    dut.expect_exact("File written")
    dut.expect_exact("Reading file")
    dut.expect_exact("Read from file:")
    dut.expect_exact("Returned from app_main")
