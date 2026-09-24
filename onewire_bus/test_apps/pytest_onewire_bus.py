# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut

@pytest.mark.generic
@pytest.mark.parametrize('config', ['rmt', 'uart'], indirect=True)
@pytest.mark.parametrize('target', ['esp32', 'esp32s2', 'esp32s3', 'esp32c3'], indirect=['target'])
def test_onewire_bus(dut: Dut, config: str) -> None:
    if config == 'rmt':
        dut.expect_exact('test-app: 1-Wire bus installed on GPIO0 by RMT backend')
    elif config == 'uart':
        dut.expect_exact('test-app: 1-Wire bus installed on GPIO0 by UART backend (UART1)')
    else:
        raise ValueError(f'Unknown test config: {config}')
    dut.expect_exact('test-app: Device iterator created, start searching')
    dut.expect_exact('test-app: Searching done')
