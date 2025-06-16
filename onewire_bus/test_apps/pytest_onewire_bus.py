# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut

@pytest.mark.generic
def test_onewire_bus(dut: Dut) -> None:
    dut.expect_exact('test-app: 1-Wire bus installed on GPIO')
    dut.expect_exact('test-app: Device iterator created, start searching')
    dut.expect_exact('test-app: Searching done')
