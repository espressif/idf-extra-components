# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD                                                                                   
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut 


@pytest.mark.linux
@pytest.mark.host_test
def test_nand_flash_linux(dut: Dut) -> None:
    dut.expect_exact('All tests passed', timeout=120)
