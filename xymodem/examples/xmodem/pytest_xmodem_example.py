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
EXAMPLE_DIR = Path(__file__).resolve().parent
HOST_TOOL = EXAMPLE_DIR / 'tools' / 'xmodem_tool.py'


def _start_host_tool(*args: str) -> subprocess.Popen:
    pytest.importorskip('serial')
    pytest.importorskip('xmodem')
    cmd = [sys.executable, str(HOST_TOOL), '-p', DATA_PORT, '--timeout', '30', *args]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    ready = proc.stdout.readline() if proc.stdout else ''
    proc.ready_line = ready  # type: ignore[attr-defined]
    if proc.poll() is not None:
        rest = proc.stdout.read() if proc.stdout else ''
        raise AssertionError((ready or '') + rest)
    return proc


def _finish_host_tool(proc: subprocess.Popen) -> None:
    try:
        out, _ = proc.communicate(timeout=60)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, _ = proc.communicate()
        log = (getattr(proc, 'ready_line', '') or '') + (out or '')
        raise AssertionError('host tool timed out\n' + log) from None
    log = (getattr(proc, 'ready_line', '') or '') + (out or '')
    logging.info('host tool exited %s:\n%s', proc.returncode, log.rstrip())
    assert proc.returncode == 0, log


@pytest.mark.usb_serial_jtag
@pytest.mark.parametrize(
    'port, flash_port, config',
    [
        pytest.param(CONSOLE_PORT, DATA_PORT, 'send'),
    ],
    indirect=True,
)
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_xmodem_example_esp_send(dut: Dut) -> None:
    dut.expect_exact("Press 's' to start...")
    host = _start_host_tool('recv', '--verify-pattern')
    try:
        dut.write('s')
        dut.expect_exact('Sending 2048 bytes (0, 1, 2, ...)')
        dut.expect_exact('Send finished', timeout=30)
    finally:
        _finish_host_tool(host)


@pytest.mark.usb_serial_jtag
@pytest.mark.parametrize(
    'port, flash_port, config',
    [
        pytest.param(CONSOLE_PORT, DATA_PORT, 'recv'),
    ],
    indirect=True,
)
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_xmodem_example_esp_recv(dut: Dut) -> None:
    dut.expect_exact("Press 's' to start...")
    host = _start_host_tool('send', '--pattern', str(PAYLOAD_LEN))
    try:
        dut.write('s')
        dut.expect_exact('Receiving. Starting handshake.')
        dut.expect_exact('Receive finished, 2048 bytes delivered to the sink (includes padding)', timeout=30)
    finally:
        _finish_host_tool(host)
