# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import http.server
import multiprocessing
import os
import socket
import ssl
import subprocess
import sys
import time
import pexpect
from typing import Any
import pytest
from pytest_embedded import Dut

server_port = 443

server_cert = (
    '-----BEGIN CERTIFICATE-----\n'
    'MIIDWDCCAkACCQCbF4+gVh/MLjANBgkqhkiG9w0BAQsFADBuMQswCQYDVQQGEwJJ\n'
    'TjELMAkGA1UECAwCTUgxDDAKBgNVBAcMA1BVTjEMMAoGA1UECgwDRVNQMQwwCgYD\n'
    'VQQLDANFU1AxDDAKBgNVBAMMA0VTUDEaMBgGCSqGSIb3DQEJARYLZXNwQGVzcC5j\n'
    'b20wHhcNMjEwNzEyMTIzNjI3WhcNNDEwNzA3MTIzNjI3WjBuMQswCQYDVQQGEwJJ\n'
    'TjELMAkGA1UECAwCTUgxDDAKBgNVBAcMA1BVTjEMMAoGA1UECgwDRVNQMQwwCgYD\n'
    'VQQLDANFU1AxDDAKBgNVBAMMA0VTUDEaMBgGCSqGSIb3DQEJARYLZXNwQGVzcC5j\n'
    'b20wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDhxF/y7bygndxPwiWL\n'
    'SwS9LY3uBMaJgup0ufNKVhx+FhGQOu44SghuJAaH3KkPUnt6SOM8jC97/yQuc32W\n'
    'ukI7eBZoA12kargSnzdv5m5rZZpd+NznSSpoDArOAONKVlzr25A1+aZbix2mKRbQ\n'
    'S5w9o1N2BriQuSzd8gL0Y0zEk3VkOWXEL+0yFUT144HnErnD+xnJtHe11yPO2fEz\n'
    'YaGiilh0ddL26PXTugXMZN/8fRVHP50P2OG0SvFpC7vghlLp4VFM1/r3UJnvL6Oz\n'
    '3ALc6dhxZEKQucqlpj8l1UegszQToopemtIj0qXTHw2+uUnkUyWIPjPC+wdOAoap\n'
    'rFTRAgMBAAEwDQYJKoZIhvcNAQELBQADggEBAItw24y565k3C/zENZlxyzto44ud\n'
    'IYPQXN8Fa2pBlLe1zlSIyuaA/rWQ+i1daS8nPotkCbWZyf5N8DYaTE4B0OfvoUPk\n'
    'B5uGDmbuk6akvlB5BGiYLfQjWHRsK9/4xjtIqN1H58yf3QNROuKsPAeywWS3Fn32\n'
    '3//OpbWaClQePx6udRYMqAitKR+QxL7/BKZQsX+UyShuq8hjphvXvk0BW8ONzuw9\n'
    'RcoORxM0FzySYjeQvm4LhzC/P3ZBhEq0xs55aL2a76SJhq5hJy7T/Xz6NFByvlrN\n'
    'lFJJey33KFrAf5vnV9qcyWFIo7PYy2VsaaEjFeefr7q3sTFSMlJeadexW2Y=\n'
    '-----END CERTIFICATE-----\n'
)

server_key = (
    '-----BEGIN PRIVATE KEY-----\n'
    'MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDhxF/y7bygndxP\n'
    'wiWLSwS9LY3uBMaJgup0ufNKVhx+FhGQOu44SghuJAaH3KkPUnt6SOM8jC97/yQu\n'
    'c32WukI7eBZoA12kargSnzdv5m5rZZpd+NznSSpoDArOAONKVlzr25A1+aZbix2m\n'
    'KRbQS5w9o1N2BriQuSzd8gL0Y0zEk3VkOWXEL+0yFUT144HnErnD+xnJtHe11yPO\n'
    '2fEzYaGiilh0ddL26PXTugXMZN/8fRVHP50P2OG0SvFpC7vghlLp4VFM1/r3UJnv\n'
    'L6Oz3ALc6dhxZEKQucqlpj8l1UegszQToopemtIj0qXTHw2+uUnkUyWIPjPC+wdO\n'
    'AoaprFTRAgMBAAECggEAE0HCxV/N1Q1h+1OeDDGL5+74yjKSFKyb/vTVcaPCrmaH\n'
    'fPvp0ddOvMZJ4FDMAsiQS6/n4gQ7EKKEnYmwTqj4eUYW8yxGUn3f0YbPHbZT+Mkj\n'
    'z5woi3nMKi/MxCGDQZX4Ow3xUQlITUqibsfWcFHis8c4mTqdh4qj7xJzehD2PVYF\n'
    'gNHZsvVj6MltjBDAVwV1IlGoHjuElm6vuzkfX7phxcA1B4ZqdYY17yCXUnvui46z\n'
    'Xn2kUTOOUCEgfgvGa9E+l4OtdXi5IxjaSraU+dlg2KsE4TpCuN2MEVkeR5Ms3Y7Q\n'
    'jgJl8vlNFJDQpbFukLcYwG7rO5N5dQ6WWfVia/5XgQKBgQD74at/bXAPrh9NxPmz\n'
    'i1oqCHMDoM9sz8xIMZLF9YVu3Jf8ux4xVpRSnNy5RU1gl7ZXbpdgeIQ4v04zy5aw\n'
    '8T4tu9K3XnR3UXOy25AK0q+cnnxZg3kFQm+PhtOCKEFjPHrgo2MUfnj+EDddod7N\n'
    'JQr9q5rEFbqHupFPpWlqCa3QmQKBgQDldWUGokNaEpmgHDMnHxiibXV5LQhzf8Rq\n'
    'gJIQXb7R9EsTSXEvsDyqTBb7PHp2Ko7rZ5YQfyf8OogGGjGElnPoU/a+Jij1gVFv\n'
    'kZ064uXAAISBkwHdcuobqc5EbG3ceyH46F+FBFhqM8KcbxJxx08objmh58+83InN\n'
    'P9Qr25Xw+QKBgEGXMHuMWgQbSZeM1aFFhoMvlBO7yogBTKb4Ecpu9wI5e3Kan3Al\n'
    'pZYltuyf+VhP6XG3IMBEYdoNJyYhu+nzyEdMg8CwXg+8LC7FMis/Ve+o7aS5scgG\n'
    '1to/N9DK/swCsdTRdzmc/ZDbVC+TuVsebFBGYZTyO5KgqLpezqaIQrTxAoGALFCU\n'
    '10glO9MVyl9H3clap5v+MQ3qcOv/EhaMnw6L2N6WVT481tnxjW4ujgzrFcE4YuxZ\n'
    'hgwYu9TOCmeqopGwBvGYWLbj+C4mfSahOAs0FfXDoYazuIIGBpuv03UhbpB1Si4O\n'
    'rJDfRnuCnVWyOTkl54gKJ2OusinhjztBjcrV1XkCgYEA3qNi4uBsPdyz9BZGb/3G\n'
    'rOMSw0CaT4pEMTLZqURmDP/0hxvTk1polP7O/FYwxVuJnBb6mzDa0xpLFPTpIAnJ\n'
    'YXB8xpXU69QVh+EBbemdJWOd+zp5UCfXvb2shAeG3Tn/Dz4cBBMEUutbzP+or0nG\n'
    'vSXnRLaxQhooWm+IuX9SuBQ=\n'
    '-----END PRIVATE KEY-----\n'
)

def get_env_config_variable(env_name, var_name):
    return os.environ.get(f'{env_name}_{var_name}'.upper())

def get_host_ip4_by_dest_ip(dest_ip):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect((dest_ip, 80))
        host_ip = s.getsockname()[0]
        s.close()
        return host_ip
    except Exception:
        return '127.0.0.1'

def _ensure_requirements_installed():
    example_dir = os.path.dirname(os.path.abspath(__file__))
    requirements_path = os.path.join(example_dir, 'tools', 'requirements.txt')

    if not os.path.exists(requirements_path):
        raise Exception(f'Requirements file not found at {requirements_path}')

    subprocess.run(
        ['pip', 'install', '-r', 'requirements.txt'],
        cwd=os.path.dirname(requirements_path),
        check=False
    )

def setting_connection(dut: Dut, env_name: str | None = None) -> Any:
    if env_name is not None and dut.app.sdkconfig.get('EXAMPLE_WIFI_SSID_PWD_FROM_STDIN') is True:
        dut.expect('Please input ssid password:')
        ap_ssid = get_env_config_variable(env_name, 'ap_ssid')
        ap_password = get_env_config_variable(env_name, 'ap_password')
        dut.write(f'{ap_ssid} {ap_password}')
    try:
        ip_address = dut.expect(r'IPv4 address: (\d+\.\d+\.\d+\.\d+)[^\d]', timeout=60)[1].decode()
        print(f'Connected to AP/Ethernet with IP: {ip_address}')
    except pexpect.exceptions.TIMEOUT:
        raise ValueError('ENV_TEST_FAILURE: Cannot connect to AP/Ethernet')
    return get_host_ip4_by_dest_ip(ip_address)

def start_https_server(ota_image_dir, server_ip, port, server_file=None, key_file=None):
    """Start an HTTPS server to serve OTA patch files."""
    os.chdir(ota_image_dir)

    if server_file is None:
        server_file = os.path.join(ota_image_dir, 'server_cert.pem')
        with open(server_file, 'w', encoding='utf-8') as f:
            f.write(server_cert)

    if key_file is None:
        key_file = os.path.join(ota_image_dir, 'server_key.pem')
        with open(key_file, 'w', encoding='utf-8') as f:  # Fixed: was 'server_key.pem' literal
            f.write(server_key)

    # Bind to all interfaces so ESP32 can reach it
    httpd = http.server.HTTPServer(('0.0.0.0', port), http.server.SimpleHTTPRequestHandler)

    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ssl_context.check_hostname = False
    ssl_context.load_cert_chain(certfile=server_file, keyfile=key_file)

    httpd.socket = ssl_context.wrap_socket(httpd.socket, server_side=True)
    httpd.serve_forever()


def find_hello_world_binary(example_dir, chip_target='esp32'):
    """
    Find the pre-built hello_world binary for the target chip.
    
    This function looks for hello_world_<target>.bin in the tests directory.
    These binaries are pre-built and checked into the repository for testing.
    
    Args:
        example_dir: Path to the example directory
        chip_target: Target chip (default: 'esp32')
    
    Returns:
        Path to the hello_world binary file
    """
    # Look for hello_world binary in tests directory
    binary_name = f'hello_world_{chip_target}.bin'
    binary_path = os.path.join(example_dir, 'tests', binary_name)
    
    if os.path.exists(binary_path):
        return binary_path
    
    # Fallback: try generic hello_world.bin
    fallback_path = os.path.join(example_dir, 'tests', 'hello_world.bin')
    if os.path.exists(fallback_path):
        print(f'Warning: Using generic hello_world.bin instead of {binary_name}')
        return fallback_path
    
    raise Exception(f'Hello world binary not found at {binary_path}. '
                   f'Expected pre-built binary: {binary_name} in tests/ directory. '
                   f'Example dir: {example_dir}')


def generate_patch(base_binary, new_binary, patch_output, chip='esp32'):
    """Generate delta OTA patch using the esp_delta_ota_patch_gen.py tool."""
    _ensure_requirements_installed()

    # Find the tool in the tools directory
    example_dir = os.path.dirname(os.path.abspath(__file__))
    tool_path = os.path.join(example_dir, 'tools', 'esp_delta_ota_patch_gen.py')
    
    if not os.path.exists(tool_path):
        raise Exception(f'Patch generation tool not found at {tool_path}')
    
    # Verify input files exist
    if not os.path.exists(base_binary):
        raise Exception(f'Base binary not found at {base_binary}')
    if not os.path.exists(new_binary):
        raise Exception(f'New binary not found at {new_binary}')
    
    # Use the tool to generate patch
    cmd = [
        sys.executable,
        tool_path,
        'create_patch',
        '--chip', chip,
        '--base_binary', base_binary,
        '--new_binary', new_binary,
        '--patch_file_name', patch_output
    ]
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    # Print output
    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print('STDERR:', result.stderr)
    
    if result.returncode != 0:
        raise Exception(f'Patch generation failed with return code {result.returncode}')
    
    if not os.path.exists(patch_output):
        raise Exception(f'Patch file not created at {patch_output}')
    
    print(f'Patch created successfully: {patch_output} ({os.path.getsize(patch_output)} bytes)')


@pytest.mark.parametrize('target', ['esp32'])
@pytest.mark.ethernet
def test_esp_delta_ota(dut: Dut):
    example_dir = os.path.dirname(os.path.abspath(__file__))
    build_dir = dut.app.binary_path
    chip_target = getattr(dut, 'target', None) or os.environ.get('IDF_TARGET', 'esp32')
    
    server_process = None
    server_process = multiprocessing.Process(
        target=start_https_server,
        args=(build_dir, '', server_port)
    )
    server_process.daemon = True
    server_process.start()
    print('HTTPS server started')
        
    try:
        base_binary = os.path.join(build_dir, 'https_delta_ota.bin')
        if not os.path.exists(base_binary):
            raise Exception(f'Base binary not found at {base_binary}. Device was flashed from build directory: {build_dir}')

        binary_name = f'hello_world_{chip_target}.bin'
        new_binary = os.path.join(example_dir, 'tests', binary_name)

        if not os.path.exists(new_binary):
            raise Exception(f'New binary not found at {new_binary}. Expected pre-built binary: {binary_name} in tests/ directory. Example dir: {example_dir}')

        # Step 2: Generate patch
        patch_file = os.path.join(build_dir, 'patch.bin')
        generate_patch(base_binary, new_binary, patch_file, chip_target)

        # Step 1: Connect device and get the correct host IP on same subnet
        env_name = 'wifi_high_traffic' if dut.app.sdkconfig.get('EXAMPLE_WIFI_SSID_PWD_FROM_STDIN') is True else None
        host_ip = setting_connection(dut, env_name)
        print(f'Device connected. Host IP on same subnet: {host_ip}')

        # Step 3: Provide OTA URL to device
        ota_url = f'https://{host_ip}:{server_port}/patch.bin'
        print(f'Providing OTA URL to device: {ota_url}')
        dut.expect('Reading OTA URL from stdin', timeout=60)
        dut.write(f'{ota_url}\n')

        # Step 4: Wait for OTA to start and complete
        dut.expect('Rebooting in', timeout=60)  # Device preparing to reboot
        
        # Step 5: Wait for reboot and new firmware to boot
        dut.expect('Hello world!', timeout=60)
        
        print('Delta OTA test PASSED: Successfully updated from https_delta_ota to hello_world')
        
    except Exception as e:
        print(f'HTTPS Delta OTA test FAILED: {str(e)}')
        raise
    finally:
        # Cleanup server process even on failure
        if server_process is not None:
            server_process.terminate()
            server_process.join(timeout=5)
            if server_process.is_alive():
                server_process.kill()
