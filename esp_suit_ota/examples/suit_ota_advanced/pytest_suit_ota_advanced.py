# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
"""Network test of the advanced example. It needs a board on a network, so it runs by hand.

The test makes two releases with esp-suit from the example's own app binary, serves them from this
machine over HTTP, and types each manifest URL on the device console:

  1. a release that updates f1 without app, which the group "cal" refuses;
  2. a release of app, f1 and cfg. The app is already installed, so only f1 and cfg are fetched,
     cfg is copied into place, and the data release is confirmed.

Run it:
  SUIT_OTA_TEST=1 SUIT_OTA_TEST_WIFI="<ssid> <password>" pytest --target esp32c5   (Wi-Fi)
  SUIT_OTA_TEST=1 pytest --target esp32p4                                          (Ethernet)
"""
import contextlib
import functools
import http.server
import os
import shutil
import socket
import subprocess
import sys
import threading

import pytest
from pytest_embedded import Dut

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, '..', '..', 'tools', 'esp_suit.py')
KEY = os.path.join(HERE, 'main', 'keys', 'manifest_key.pem')
RELEASE = os.path.join(HERE, 'release')


def esp_suit(*args: str) -> None:
    subprocess.check_call([sys.executable, TOOL, *args])


def make_release(work: str, name: str, seq: int, components: list) -> None:
    """components: (pair name, payload file, skip_if_installed). Writes <name>.suit next to the payloads."""
    lines = [f'sequence_number: {seq}', 'vendor: espressif.com', 'class: suit-ota-example', 'components:']
    for pair, path, skip in components:
        shutil.copyfile(path, os.path.join(work, f'{pair}.bin'))
        lines.append(f'  - {{ id: [esp32c5, {pair}], file: "{pair}.bin", uri: {pair}.bin, '
                     f'skip_if_installed: {str(skip).lower()} }}')
    lines += ['signing:', f'  keys: ["{KEY}"]', '']
    yml = os.path.join(work, f'{name}.yml')
    with open(yml, 'w') as f:
        f.write('\n'.join(lines))
    esp_suit('create', yml)
    esp_suit('verify', os.path.join(work, f'{name}.suit'), '--key', KEY,
             '--vendor', 'espressif.com', '--class', 'suit-ota-example')


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args) -> None:
        pass


@contextlib.contextmanager
def serve(directory: str):
    server = http.server.ThreadingHTTPServer(('0.0.0.0', 0), functools.partial(QuietHandler, directory=directory))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server.server_address[1]
    finally:
        server.shutdown()


def host_ip_towards(ip: str) -> str:
    """The address of this machine on the interface that reaches ip."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.connect((ip, 80))
        return s.getsockname()[0]


@pytest.mark.skipif(not os.environ.get('SUIT_OTA_TEST'),
                    reason='needs a board on a network, which this CI does not have: set SUIT_OTA_TEST=1')
@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32c5', 'esp32p4'], indirect=['target'])
@pytest.mark.parametrize('config', ['default'], indirect=['config'])
def test_suit_ota_advanced(dut: Dut, tmp_path) -> None:
    work = str(tmp_path)
    app = os.path.join(dut.app.binary_path, 'suit_ota_advanced.bin')
    make_release(work, 'partial_release', 2003, [('f1', os.path.join(RELEASE, 'f1_v1.bin'), False)])
    make_release(work, 'release', 2001, [('app', app, True),
                                         ('f1', os.path.join(RELEASE, 'f1_v2.bin'), True),
                                         ('cfg', os.path.join(RELEASE, 'cfg_v1.bin'), True)])

    # a first boot: no slots record, no installed release, blank data partitions
    for part in ('nvs', 'f1_live', 'f1_spare', 'cfg', 'cfg_stage'):
        dut.serial.erase_partition(part)
    dut.serial.hard_reset()

    if dut.target != 'esp32p4':
        wifi = os.environ.get('SUIT_OTA_TEST_WIFI')
        if not wifi:
            pytest.skip('set SUIT_OTA_TEST_WIFI="<ssid> <password>" for a Wi-Fi target')
        dut.expect('Please input ssid password', timeout=30)
        dut.write(wifi)
    ip = dut.expect(r'IPv4 address: (\d+\.\d+\.\d+\.\d+)[^\d]', timeout=60).group(1).decode()
    host = host_ip_towards(ip)

    with serve(work) as port:
        dut.expect('Enter the manifest URL', timeout=30)
        dut.write(f'http://{host}:{port}/partial_release.suit')
        dut.expect('RESULT: failed: ESP_ERR_SUIT_OTA_GROUP', timeout=60)

        dut.expect('Enter the manifest URL', timeout=30)
        dut.write(f'http://{host}:{port}/release.suit')
        dut.expect('esp32c5/app already installed', timeout=60)
        dut.expect('esp32c5/f1 staged and verified', timeout=120)
        dut.expect('esp32c5/cfg staged and verified', timeout=60)
        dut.expect('esp32c5/cfg: copying cfg_stage to cfg', timeout=30)
        dut.expect('release 2001 activated', timeout=30)
        dut.expect('RESULT: data release confirmed', timeout=30)
