# SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut


@pytest.mark.host_test
@pytest.mark.parametrize('target', ['linux'], indirect=['target'])
def test_catch2_example(dut: Dut) -> None:
    dut.expect_exact('All tests passed')
    dut.expect_exact('1 assertion in 1 test case')
