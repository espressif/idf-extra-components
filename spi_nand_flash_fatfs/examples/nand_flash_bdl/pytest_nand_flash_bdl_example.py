# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import glob
from pathlib import Path

import pytest
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.spi_nand_flash
@pytest.mark.skipif(
    not bool(glob.glob(f'{Path(__file__).parent.absolute()}/build*/')),
    reason='Skip when example has not been built (no build*/ directory)',
)
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_nand_flash_bdl_example(dut) -> None:
    dut.expect_exact('Pre-mount format finished')
    dut.expect_exact('Opening file')
    dut.expect_exact('File written')
    dut.expect_exact('Reading file')
    dut.expect_exact('Read from file:')
    dut.expect_exact('Unmounting FAT filesystem')
    dut.expect_exact('Releasing BDL stack (WL releases underlying flash BDL)')
    dut.expect_exact('Done')
    dut.expect_exact('Returned from app_main')
