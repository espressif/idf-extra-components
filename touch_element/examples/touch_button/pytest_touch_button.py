# SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut


@pytest.mark.generic
@pytest.mark.skip_if_soc("IDF_VERSION_MAJOR <= 5 and IDF_VERSION_MINOR < 3")
@pytest.mark.parametrize('target', ['esp32s2', 'esp32s3'], indirect=['target'])
def test_touch_button(dut: Dut) -> None:
    dut.expect_exact('Touch Button Example: Touch element library installed')
    dut.expect_exact('Touch Button Example: Touch button installed')
    dut.expect_exact('Touch Button Example: Touch buttons created')
    dut.expect_exact('Touch Button Example: Touch element library start')
