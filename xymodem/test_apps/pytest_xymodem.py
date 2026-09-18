# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest


@pytest.mark.generic
def test_xymodem(dut) -> None:
    dut.run_all_single_board_cases()
