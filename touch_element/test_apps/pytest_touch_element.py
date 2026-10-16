# SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut
import glob
from pathlib import Path


@pytest.mark.disable_test(
    'IDF_TARGET == "esp32s3" and CONFIG_NAME == "defaults"',
    reason='This configuration uses USB Serial/JTAG console, but the generic runner reads UART; disable execution until console routing is configured',
)
@pytest.mark.generic
@pytest.mark.parametrize('config', ['defaults'], indirect=['config'])
@pytest.mark.parametrize('target', ['esp32s2', 'esp32s3'], indirect=['target'])
def test_touch_element(dut: Dut) -> None:
    dut.run_all_single_board_cases(timeout=120)
