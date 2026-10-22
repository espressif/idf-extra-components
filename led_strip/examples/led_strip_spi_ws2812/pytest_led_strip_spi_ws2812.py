# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
@pytest.mark.parametrize(
    'target',
    ['esp32', 'esp32s3', 'esp32c3', 'esp32c5'],
    indirect=['target'],
)
def test_led_strip_spi_ws2812(dut: Dut) -> None:
    """Test that the SPI LED strip example starts and blinks."""
    dut.expect_exact('Created LED strip object with SPI backend')
    dut.expect_exact('Start blinking LED strip')
    dut.expect_exact('LED OFF!')
    dut.expect_exact('LED ON!')
