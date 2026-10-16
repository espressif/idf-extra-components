# SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut


@pytest.mark.disable_test(
    'IDF_TARGET in ["esp32c3", "esp32c5"]',
    reason='Preserve the previous CI runtime coverage; enable additional hardware targets after runner validation',
)
@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32c3', 'esp32c5'], indirect=['target'])
def test_lua_blink(dut: Dut) -> None:
    dut.expect_exact('Lua Blink Example Starting')
    dut.expect_exact('Initializing LittleFS filesystem')
    dut.expect_exact('Filesystem mounted at /assets')
    dut.expect_exact('Created LED strip object with RMT backend')
    dut.expect_exact('Starting Lua script from file: blink_led.lua')
    # The script drives the LED strip through the C API: ON/OFF a few times.
    dut.expect_exact('Blinking LED 0 on the strip, 5 times')
    for _ in range(5):
        dut.expect_exact('LED ON')
        dut.expect_exact('LED OFF')
    dut.expect_exact('Blinking finished, free heap: ')
    dut.expect_exact('End of Lua script from file: blink_led.lua')
    dut.expect_exact('End of Lua blink example application.')
