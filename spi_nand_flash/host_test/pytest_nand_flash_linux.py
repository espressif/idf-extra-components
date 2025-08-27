# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.host_test
@idf_parametrize('target', ['linux'], indirect=['target'])
@pytest.mark.skip_if_soc("IDF_VERSION_MAJOR <= 5 and IDF_VERSION_MINOR < 3") # Linux apps not built for 5.1~5.2
def test_nand_flash_linux(dut: Dut) -> None:
    dut.expect_exact('All tests passed', timeout=120)
