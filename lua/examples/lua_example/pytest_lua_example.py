# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
def test_lua_example(dut: Dut) -> None:
    dut.expect_exact('Lua Example Starting')
    dut.expect_exact('Initializing LittleFS filesystem')
    dut.expect_exact('Filesystem mounted at /assets')
    dut.expect_exact('Starting Lua test: Simple Embedded Script')
    dut.expect_exact('The answer is: 42')
    dut.expect_exact('End of Lua test: Simple Embedded Script')
    dut.expect_exact('Starting Lua test from file: Fibonacci Script from File')
    dut.expect_exact('Fibonacci of 10 is: 55')
    dut.expect_exact('End of Lua test from file: Fibonacci Script from File')
    dut.expect_exact('End of Lua example application.')
