# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import http.server
import multiprocessing
import os
import sys

import socket
import ssl
from typing import Optional
from typing import Callable

import pexpect
import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.app import FlashFile
from pytest_embedded_idf.serial import IdfSerial

enc_bin_name = 'pre_encrypted_ota_secure.bin'
host_ip = '127.0.0.1'
server_port = 443

server_cert = '-----BEGIN CERTIFICATE-----\n'\
             'MIIDKzCCAhOgAwIBAgIUBxM3WJf2bP12kAfqhmhhjZWv0ukwDQYJKoZIhvcNAQEL\n'\
             'BQAwJTEjMCEGA1UEAwwaRVNQMzIgSFRUUFMgc2VydmVyIGV4YW1wbGUwHhcNMTgx\n'\
             'MDE3MTEzMjU3WhcNMjgxMDE0MTEzMjU3WjAlMSMwIQYDVQQDDBpFU1AzMiBIVFRQ\n'\
             'UyBzZXJ2ZXIgZXhhbXBsZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n'\
             'ALBint6nP77RCQcmKgwPtTsGK0uClxg+LwKJ3WXuye3oqnnjqJCwMEneXzGdG09T\n'\
             'sA0SyNPwrEgebLCH80an3gWU4pHDdqGHfJQa2jBL290e/5L5MB+6PTs2NKcojK/k\n'\
             'qcZkn58MWXhDW1NpAnJtjVniK2Ksvr/YIYSbyD+JiEs0MGxEx+kOl9d7hRHJaIzd\n'\
             'GF/vO2pl295v1qXekAlkgNMtYIVAjUy9CMpqaQBCQRL+BmPSJRkXBsYk8GPnieS4\n'\
             'sUsp53DsNvCCtWDT6fd9D1v+BB6nDk/FCPKhtjYOwOAZlX4wWNSZpRNr5dfrxKsb\n'\
             'jAn4PCuR2akdF4G8WLUeDWECAwEAAaNTMFEwHQYDVR0OBBYEFMnmdJKOEepXrHI/\n'\
             'ivM6mVqJgAX8MB8GA1UdIwQYMBaAFMnmdJKOEepXrHI/ivM6mVqJgAX8MA8GA1Ud\n'\
             'EwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEBADiXIGEkSsN0SLSfCF1VNWO3\n'\
             'emBurfOcDq4EGEaxRKAU0814VEmU87btIDx80+z5Dbf+GGHCPrY7odIkxGNn0DJY\n'\
             'W1WcF+DOcbiWoUN6DTkAML0SMnp8aGj9ffx3x+qoggT+vGdWVVA4pgwqZT7Ybntx\n'\
             'bkzcNFW0sqmCv4IN1t4w6L0A87ZwsNwVpre/j6uyBw7s8YoJHDLRFT6g7qgn0tcN\n'\
             'ZufhNISvgWCVJQy/SZjNBHSpnIdCUSJAeTY2mkM4sGxY0Widk8LnjydxZUSxC3Nl\n'\
             'hb6pnMh3jRq4h0+5CZielA4/a+TdrNPv/qok67ot/XJdY3qHCCd8O2b14OVq9jo=\n'\
             '-----END CERTIFICATE-----\n'

server_key = '-----BEGIN PRIVATE KEY-----\n'\
             'MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCwYp7epz++0QkH\n'\
             'JioMD7U7BitLgpcYPi8Cid1l7snt6Kp546iQsDBJ3l8xnRtPU7ANEsjT8KxIHmyw\n'\
             'h/NGp94FlOKRw3ahh3yUGtowS9vdHv+S+TAfuj07NjSnKIyv5KnGZJ+fDFl4Q1tT\n'\
             'aQJybY1Z4itirL6/2CGEm8g/iYhLNDBsRMfpDpfXe4URyWiM3Rhf7ztqZdveb9al\n'\
             '3pAJZIDTLWCFQI1MvQjKamkAQkES/gZj0iUZFwbGJPBj54nkuLFLKedw7DbwgrVg\n'\
             '0+n3fQ9b/gQepw5PxQjyobY2DsDgGZV+MFjUmaUTa+XX68SrG4wJ+DwrkdmpHReB\n'\
             'vFi1Hg1hAgMBAAECggEAaTCnZkl/7qBjLexIryC/CBBJyaJ70W1kQ7NMYfniWwui\n'\
             'f0aRxJgOdD81rjTvkINsPp+xPRQO6oOadjzdjImYEuQTqrJTEUnntbu924eh+2D9\n'\
             'Mf2CAanj0mglRnscS9mmljZ0KzoGMX6Z/EhnuS40WiJTlWlH6MlQU/FDnwC6U34y\n'\
             'JKy6/jGryfsx+kGU/NRvKSru6JYJWt5v7sOrymHWD62IT59h3blOiP8GMtYKeQlX\n'\
             '49om9Mo1VTIFASY3lrxmexbY+6FG8YO+tfIe0tTAiGrkb9Pz6tYbaj9FjEWOv4Vc\n'\
             '+3VMBUVdGJjgqvE8fx+/+mHo4Rg69BUPfPSrpEg7sQKBgQDlL85G04VZgrNZgOx6\n'\
             'pTlCCl/NkfNb1OYa0BELqWINoWaWQHnm6lX8YjrUjwRpBF5s7mFhguFjUjp/NW6D\n'\
             '0EEg5BmO0ePJ3dLKSeOA7gMo7y7kAcD/YGToqAaGljkBI+IAWK5Su5yldrECTQKG\n'\
             'YnMKyQ1MWUfCYEwHtPvFvE5aPwKBgQDFBWXekpxHIvt/B41Cl/TftAzE7/f58JjV\n'\
             'MFo/JCh9TDcH6N5TMTRS1/iQrv5M6kJSSrHnq8pqDXOwfHLwxetpk9tr937VRzoL\n'\
             'CuG1Ar7c1AO6ujNnAEmUVC2DppL/ck5mRPWK/kgLwZSaNcZf8sydRgphsW1ogJin\n'\
             '7g0nGbFwXwKBgQCPoZY07Pr1TeP4g8OwWTu5F6dSvdU2CAbtZthH5q98u1n/cAj1\n'\
             'noak1Srpa3foGMTUn9CHu+5kwHPIpUPNeAZZBpq91uxa5pnkDMp3UrLIRJ2uZyr8\n'\
             '4PxcknEEh8DR5hsM/IbDcrCJQglM19ZtQeW3LKkY4BsIxjDf45ymH407IQKBgE/g\n'\
             'Ul6cPfOxQRlNLH4VMVgInSyyxWx1mODFy7DRrgCuh5kTVh+QUVBM8x9lcwAn8V9/\n'\
             'nQT55wR8E603pznqY/jX0xvAqZE6YVPcw4kpZcwNwL1RhEl8GliikBlRzUL3SsW3\n'\
             'q30AfqEViHPE3XpE66PPo6Hb1ymJCVr77iUuC3wtAoGBAIBrOGunv1qZMfqmwAY2\n'\
             'lxlzRgxgSiaev0lTNxDzZkmU/u3dgdTwJ5DDANqPwJc6b8SGYTp9rQ0mbgVHnhIB\n'\
             'jcJQBQkTfq6Z0H6OoTVi7dPs3ibQJFrtkoyvYAbyk36quBmNRjVh6rc8468bhXYr\n'\
             'v/t+MeGJP/0Zw8v/X2CFll96\n'\
             '-----END PRIVATE KEY-----\n'

def start_https_server(ota_image_dir: str, server_ip: str, port: int, server_file: Optional[str] = None, key_file: Optional[str] = None) -> None:
    os.chdir(ota_image_dir)

    if server_file is None:
        server_file = os.path.join(ota_image_dir, 'server_cert.pem')
        cert_file_handle = open(server_file, 'w+')
        cert_file_handle.write(server_cert)
        cert_file_handle.close()

    if key_file is None:
        key_file = os.path.join(ota_image_dir, 'server_key.pem')
        key_file_handle = open('server_key.pem', 'w+')
        key_file_handle.write(server_key)
        key_file_handle.close()

    httpd = http.server.HTTPServer((server_ip, port), http.server.SimpleHTTPRequestHandler)

    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ssl_context.load_cert_chain(certfile=server_file, keyfile=key_file)

    httpd.socket = ssl_context.wrap_socket(httpd.socket, server_side=True)
    httpd.serve_forever()

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

if __name__ == '__main__':
    if sys.argv[2:]: # if two or more arguments are provided
        # Usage: python pytest_pre_encrypted_ota.py <image_dir> <server_port> <cert_dir>
        this_dir = os.path.dirname(os.path.realpath(__file__))
        bin_dir = os.path.join(this_dir, sys.argv[1])
        port = int(sys.argv[2])
        cert_dir = bin_dir if not sys.argv[3:] else os.path.join(this_dir, sys.argv[3])  # optional argument
        print(f'Starting HTTPS server at "https://0.0.0.0:{port}"')
        server_file=os.path.join(cert_dir, 'ca_cert.pem')
        key_file=os.path.join(cert_dir, 'ca_key.pem')
        #check if file exits
        if not os.path.exists(server_file):
            server_file = None
        if not os.path.exists(key_file):
            key_file = None

        start_https_server(bin_dir, '', port, server_file, key_file)
