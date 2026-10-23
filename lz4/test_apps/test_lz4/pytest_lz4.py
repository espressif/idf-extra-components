# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
@pytest.mark.parametrize(
    'port, flash_port',
    [pytest.param('/dev/ttyUSB0', '/dev/ttyUSB0', id='ttyUSB')],
    indirect=True,
)
@pytest.mark.parametrize('target', ['esp32', 'esp32c3', 'esp32c6'], indirect=['target'])
def test_lz4(dut: Dut) -> None:
    """Run the Unity suite, including codec integration and benchmark tests."""
    dut.run_all_single_board_cases(timeout=60)
