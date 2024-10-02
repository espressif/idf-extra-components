# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
def test_catch2_console_example(dut: Dut) -> None:
    dut.expect_exact('Type \'help\' to get the list of commands.')
    dut.write('test -?\n')
    dut.expect_exact('For more detailed usage please see the project docs')
    dut.write('test\n')
    dut.expect_exact('All tests passed')
    dut.expect_exact('1 assertion in 1 test case')
