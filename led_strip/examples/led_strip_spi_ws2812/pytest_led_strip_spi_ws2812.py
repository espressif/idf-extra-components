# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut

@pytest.mark.generic
def test_led_strip_spi_ws2812(dut: Dut) -> None:
    dut.expect_exact('example: Created LED strip object with SPI backend')
    dut.expect_exact('example: Start blinking LED strip')
    dut.expect_exact('example: LED OFF!')
    dut.expect_exact('example: LED ON!')

