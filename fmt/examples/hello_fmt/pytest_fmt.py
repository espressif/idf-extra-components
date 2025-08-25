# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32'], indirect=True)
def test_fmt_example(dut: Dut) -> None:
    dut.expect_exact('Hello, fmt!')
