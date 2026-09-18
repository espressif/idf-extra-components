# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
"""USB Serial/JTAG console; XMODEM data on the USB-UART adapter (UART0)."""

import logging
import subprocess
import sys
from pathlib import Path

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize

PAYLOAD_LEN = 2048
CONSOLE_PORT = '/dev/serial_ports/ttyACM-esp32'
DATA_PORT = '/dev/serial_ports/ttyUSB-esp32'
HOST_TOOL = Path(__file__).resolve().parent / 'tools' / 'xmodem_tool.py'


@pytest.mark.usb_serial_jtag
@pytest.mark.parametrize(
    'port, flash_port, config',
    [pytest.param(CONSOLE_PORT, DATA_PORT, 'echo')],
    indirect=True,
)
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_xmodem_example_echo(dut: Dut) -> None:
    pytest.importorskip('serial')
    pytest.importorskip('xmodem')

    dut.expect_exact('Receiving into 2048-byte buffer')
    host = subprocess.Popen(
        [sys.executable, str(HOST_TOOL), '-p', DATA_PORT, str(PAYLOAD_LEN)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        dut.expect_exact('Received packet 1, 1024 bytes')
        dut.expect_exact('Received packet 2, 1024 bytes')
        dut.expect_exact('Receive finished, 2048 bytes', timeout=30)
        dut.expect_exact('Sending packet 1, 1024 bytes')
        dut.expect_exact('Sending packet 2, 1024 bytes')
        dut.expect_exact('Send finished', timeout=30)
    finally:
        out, _ = host.communicate(timeout=60)
        logging.info('host tool exited %s:\n%s', host.returncode, (out or '').rstrip())
        assert host.returncode == 0, out
