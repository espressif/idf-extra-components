# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut


@pytest.mark.host_test
@pytest.mark.parametrize('target', ['linux'], indirect=['target'])
def test_expat_host(dut: Dut) -> None:
    dut.expect_exact("Running expat component tests")
    dut.run_all_single_board_cases()


@pytest.mark.generic
@pytest.mark.parametrize(
    'port, flash_port',
    [pytest.param('/dev/ttyUSB0', '/dev/ttyUSB0', id='ttyUSB')],
    indirect=True,
)
@pytest.mark.parametrize('target', ['esp32s3', 'esp32c6'], indirect=['target'])
def test_expat_on_chip(dut: Dut) -> None:
    dut.expect_exact("Running expat component tests")
    dut.run_all_single_board_cases()
