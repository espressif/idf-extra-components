# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
@pytest.mark.parametrize(
    'port, flash_port',
    [pytest.param('/dev/ttyUSB0', '/dev/ttyUSB0', id='ttyUSB')],
    indirect=True,
)
@pytest.mark.parametrize('target', ['esp32s3', 'esp32c6'], indirect=['target'])
def test_zlib(dut: Dut) -> None:
    """Run the Unity suite: codec round trips, and PSRAM placement when enabled."""
    dut.expect_exact('Running zlib component tests')
    dut.run_all_single_board_cases(timeout=60)
