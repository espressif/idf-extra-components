# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import http.server
import multiprocessing
import os
import socket
import ssl
from typing import Callable

import pexpect
import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.app import FlashFile
from pytest_embedded_idf.serial import IdfSerial

enc_bin_name = 'pre_encrypted_ota_secure.bin'
host_ip = '127.0.0.1'
server_port = 443

@pytest.mark.generic
def test_examples_protocol_pre_encrypted_ota_example(dut: Dut) -> None:
    bin_path = os.path.join(dut.app.binary_path, enc_bin_name)
    bin_size = os.path.getsize(bin_path)
    # Construct the URI
    uri = f'https://{host_ip}:{server_port}/'
    
    try:
        dut.expect('Loaded app from partition at offset', timeout=30)
        dut.expect('Starting Pre Encrypted OTA example', timeout=30)
        dut.write(f'{uri} {bin_size}\n')
        dut.expect('Magic Verified', timeout=30)
        dut.expect('Reading RSA private key', timeout=30)
        dut.expect('upgrade successful. Rebooting', timeout=60)
        # after reboot
        dut.expect('Loaded app from partition at offset', timeout=30)
    finally:
        pass

