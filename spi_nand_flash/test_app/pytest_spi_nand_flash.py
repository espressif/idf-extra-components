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
@pytest.mark.parametrize(
    'config',
    [
        'default',
    ],
    indirect=True,
)
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_spi_nand_flash(dut: Dut, config: str) -> None:
    dut.run_all_single_board_cases()


@pytest.mark.spi_nand_flash
@pytest.mark.skipif(
    not bool(glob.glob(f'{Path(__file__).parent.absolute()}/build*/')),
    reason="Skip the idf version that not build"
)
@pytest.mark.parametrize(
    'config',
    [
        'bdl',
    ],
    indirect=True,
)
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_spi_nand_flash_bdl(dut: Dut, config: str) -> None:
    dut.run_all_single_board_cases()
