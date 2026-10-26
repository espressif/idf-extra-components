# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut


@pytest.mark.generic
@pytest.mark.parametrize('port', [pytest.param('/dev/ttyUSB*', id='ttyUSB')], indirect=True)
@pytest.mark.parametrize('target', ['esp32', 'esp32s3', 'esp32p4'], indirect=['target'])
def test_esp_flash_dispatcher_generic(dut) -> None:
    dut.run_all_single_board_cases()
