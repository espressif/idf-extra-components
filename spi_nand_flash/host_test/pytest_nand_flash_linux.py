# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize
import glob
from pathlib import Path


@pytest.mark.host_test
@pytest.mark.skipif(
    not bool(glob.glob(f'{Path(__file__).parent.absolute()}/build*/')),
    reason="Skip the idf version that not build"
)
@idf_parametrize('target', ['linux'], indirect=['target'])
def test_nand_flash_linux(dut: Dut) -> None:
    dut.expect_exact('All tests passed', timeout=120)
