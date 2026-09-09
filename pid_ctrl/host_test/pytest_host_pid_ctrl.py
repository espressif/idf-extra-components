# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut


@pytest.mark.host_test
@pytest.mark.parametrize('target', ['linux'], indirect=['target'])
def test_pid_ctrl(dut: Dut) -> None:
    dut.run_all_single_board_cases()
