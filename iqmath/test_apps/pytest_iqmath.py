# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest

@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32'], indirect=True)
def test_iqmath(dut) -> None:
    dut.run_all_single_board_cases()