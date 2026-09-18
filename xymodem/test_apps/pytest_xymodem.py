# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded_idf.utils import idf_parametrize

UART_PORT = '/dev/ttyUSB*'

@pytest.mark.generic
@pytest.mark.parametrize(
    'port, flash_port',
    [
        pytest.param(UART_PORT, UART_PORT, id='uart'),
    ],
    indirect=True,
)
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_xymodem(dut) -> None:
    dut.run_all_single_board_cases()
