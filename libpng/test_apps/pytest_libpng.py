# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32c3', 'esp32c5'], indirect=['target'])
def test_libpng(dut: Dut) -> None:
    dut.run_all_single_board_cases()
