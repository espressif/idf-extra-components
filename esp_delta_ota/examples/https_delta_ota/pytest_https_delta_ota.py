# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import http.server
import multiprocessing
import os
import shutil
import socket
import ssl
import subprocess
import sys
import time
import pexpect
from typing import Any
import pytest
from pytest_embedded import Dut

# Try to import common_test_methods - it should be available via pytest-embedded-idf in CI
# If not available, try to find it via IDF_PATH (for local development)
# Provide fallback implementations that work without IDF_PATH
_common_test_methods_imported = False
try:
    from common_test_methods import get_env_config_variable
    from common_test_methods import get_host_ip4_by_dest_ip
    _common_test_methods_imported = True
except (ModuleNotFoundError, ImportError):
    # Try to find via IDF_PATH (for local development)
    idf_path = os.environ.get('IDF_PATH')
    if idf_path and os.path.exists(idf_path):
        ci_tools_path = os.path.join(idf_path, 'tools', 'ci')
        if os.path.exists(ci_tools_path):
            sys.path.insert(0, ci_tools_path)
            python_packages_path = os.path.join(idf_path, 'tools', 'ci', 'python_packages')
            if os.path.exists(python_packages_path):
                sys.path.insert(0, python_packages_path)
            try:
                from common_test_methods import get_env_config_variable
                from common_test_methods import get_host_ip4_by_dest_ip
                _common_test_methods_imported = True
            except (ModuleNotFoundError, ImportError):
                pass

# Define fallback implementations if import failed
if not _common_test_methods_imported:
    def get_env_config_variable(env_name, var_name):
        """Fallback implementation - returns environment variable or None"""
        return os.environ.get(f'{env_name}_{var_name}'.upper())
    
    def get_host_ip4_by_dest_ip(dest_ip):
        """Fallback implementation - tries to get host IP on same subnet as destination"""
        import socket
        try:
            # Try to get host IP on same subnet as destination
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect((dest_ip, 80))
            host_ip = s.getsockname()[0]
            s.close()
            return host_ip
        except Exception:
            # Fallback to localhost if we can't determine the IP
            return '127.0.0.1'

# Track if requirements are installed to avoid repeated checks
_requirements_installed = False

def _ensure_requirements_installed():
    """Ensure detools and dependencies are installed from tools/requirements.txt
    
    This function is called lazily when needed, not at module import time,
    to avoid breaking pytest collection in environments where dependencies
    can't be installed (e.g., Linux target CI without build tools).
    """
    global _requirements_installed
    if _requirements_installed:
        return
    
    # Check if detools is already available
    try:
        import detools  # noqa: F401
        import esptool  # noqa: F401
        _requirements_installed = True
        return  # Dependencies already available
    except ImportError:
        pass
    
    # Install dependencies if not available
    example_dir = os.path.dirname(os.path.abspath(__file__))
    requirements_path = os.path.join(example_dir, 'tools', 'requirements.txt')
    
    if not os.path.exists(requirements_path):
        raise Exception(f'Requirements file not found at {requirements_path}')
    
    # Install using the same Python interpreter with --user flag for CI compatibility
    # Try --user first (works in CI), fallback to regular install if that fails
    install_cmd = [sys.executable, '-m', 'pip', 'install', '--user', '--quiet', '-r', requirements_path]
    result = subprocess.run(install_cmd, capture_output=True, text=True)
    
    if result.returncode != 0:
        # Fallback: try without --user flag
        install_cmd = [sys.executable, '-m', 'pip', 'install', '--quiet', '-r', requirements_path]
        result = subprocess.run(install_cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            # Verify if packages are now available (might have been installed by CI)
            try:
                import detools  # noqa: F401
                import esptool  # noqa: F401
                _requirements_installed = True
                return  # Dependencies are available despite install failure
            except ImportError:
                # Installation failed - this will be caught when the test actually runs
                # Don't raise here to allow pytest collection to succeed
                print(f'Warning: Failed to install requirements. Dependencies will be checked when test runs.')
                print(f'Install error: {result.stderr[:500]}')  # Print first 500 chars
                # Mark as attempted so we don't retry
                _requirements_installed = True

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


def find_binary_from_build_dir(example_dir, build_config_suffix, chip_target='esp32'):
    """
    Find the binary file from a build directory based on config suffix.
    
    This function searches for binaries built with sdkconfig.ci.base and sdkconfig.ci.new
    configurations. The build directories follow the pattern: build_<target>_<config_suffix>
    (e.g., build_esp32_base, build_esp32_new).
    
    Args:
        example_dir: Path to the example directory
        build_config_suffix: Suffix of the build directory (e.g., 'base' or 'new')
        chip_target: Target chip (default: 'esp32')
    
    Returns:
        Path to the binary file
    """
    import glob
    
    # Build directory naming: build_@t_@w where @t is target and @w is config name
    # For sdkconfig.ci.base -> build_esp32_base
    # For sdkconfig.ci.new -> build_esp32_new
    build_dir_name = f'build_{chip_target}_{build_config_suffix}'
    binary_name = 'https_delta_ota.bin'
    
    # First, try the direct path in the example directory
    binary_path = os.path.join(example_dir, build_dir_name, binary_name)
    if os.path.exists(binary_path):
        return binary_path
    
    # Search in the example directory recursively
    pattern = os.path.join(example_dir, '**', build_dir_name, binary_name)
    matches = glob.glob(pattern, recursive=True)
    if matches:
        return matches[0]
    
    # Search in parent directories (for CI artifact structure)
    # Artifacts might be at workspace root: esp_delta_ota/examples/https_delta_ota/build_esp32_base/https_delta_ota.bin
    parent_dir = os.path.dirname(example_dir)
    if parent_dir:
        pattern = os.path.join(parent_dir, '**', build_dir_name, binary_name)
        matches = glob.glob(pattern, recursive=True)
        if matches:
            return matches[0]
    
    # Also search from workspace root (for merged CI artifacts)
    # Pattern: */examples/*/build_esp*_base/https_delta_ota.bin
    workspace_root = os.path.abspath(os.path.join(example_dir, '..', '..', '..'))
    pattern = os.path.join(workspace_root, '**', build_dir_name, binary_name)
    matches = glob.glob(pattern, recursive=True)
    if matches:
        return matches[0]
    
    # Last resort: search from current working directory
    pattern = os.path.join('**', build_dir_name, binary_name)
    matches = glob.glob(pattern, recursive=True)
    if matches:
        return os.path.abspath(matches[0])
    
    raise Exception(f'Binary not found for build config "{build_config_suffix}" (chip: {chip_target}). '
                   f'Searched for build directory: {build_dir_name}, binary: {binary_name}. '
                   f'Example dir: {example_dir}')


def generate_patch(base_binary, new_binary, patch_output, chip='esp32'):
    """Generate delta OTA patch using the esp_delta_ota_patch_gen.py tool."""
    # Ensure dependencies are installed (lazy installation when actually needed)
    _ensure_requirements_installed()
    
    # Verify dependencies are available before proceeding
    try:
        import detools  # noqa: F401
        import esptool  # noqa: F401
    except ImportError as e:
        raise Exception(f'Required dependencies (detools, esptool) are not available. '
                       f'Please install them manually or ensure they can be installed. '
                       f'Original error: {e}')
    
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


def create_ca_cert_pem(example_dir):
    """Create ca_cert.pem file in main directory for the build."""
    ca_cert_path = os.path.join(example_dir, 'main', 'ca_cert.pem')
    with open(ca_cert_path, 'w') as f:
        f.write(server_cert)


def _delta_ota_common(dut: Dut, protocol='http', port=8070):
    """
    Common test logic for delta OTA testing.
    
    Follows ESP-IDF's simple_ota_example pattern:
    - Device boots and connects to WiFi/Ethernet
    - Test detects device IP and selects host IP on same subnet
    - Test finds pre-built binaries from build_esp*_base and build_esp*_new directories
    - Test generates delta patch from these binaries
    - Test starts HTTP/HTTPS server on detected subnet
    - Device receives OTA URL via stdin
    - Performs delta OTA update and verifies success

    Args:
        dut: Device under test
        protocol: 'http' or 'https'
        port: Server port to use
    """
    # Get paths
    example_dir = os.path.dirname(os.path.abspath(__file__))
    build_dir = dut.app.binary_path
    
    # Get the target - try from DUT first, then environment, default to esp32
    chip_target = getattr(dut, 'target', None) or os.environ.get('IDF_TARGET', 'esp32')
    
    try:
        # Step 1: Use the binary that was actually flashed on the device as the base binary
        # This is CRITICAL - the patch SHA256 must match the firmware running on the device
        # The device was flashed from build_dir (dut.app.binary_path), so use that binary
        base_binary = os.path.join(build_dir, 'https_delta_ota.bin')
        if not os.path.exists(base_binary):
            raise Exception(f'Base binary not found at {base_binary}. Device was flashed from build directory: {build_dir}')
        
        # Step 2: Find the new binary from pre-built artifacts (build_esp*_new)
        # This binary has EXAMPLE_TEST_DELTA_OTA enabled and will be the target after OTA
        print(f'Looking for new binary with chip target: {chip_target}')
        new_binary = find_binary_from_build_dir(example_dir, 'new', chip_target)
        
        print(f'Using flashed base binary (SHA256 must match device): {base_binary}')
        print(f'Found new binary (target after OTA): {new_binary}')
        
        # Step 3: Get device IP and determine correct host IP (ESP-IDF pattern)
        # This ensures host IP is on same subnet as device
        env_name = 'wifi_high_traffic' if dut.app.sdkconfig.get('EXAMPLE_WIFI_SSID_PWD_FROM_STDIN') is True else None
        host_ip = setting_connection(dut, env_name)
        
        ota_url = f'{protocol}://{host_ip}:{port}/patch.bin'
        print(f'OTA URL: {ota_url}')
        
        # Step 4: Generate delta patch from binaries
        # Base binary is the one flashed on device, new binary is from build_esp*_new
        patch_file = os.path.join(build_dir, 'patch.bin')
        generate_patch(base_binary, new_binary, patch_file, chip_target)

        server_process = multiprocessing.Process(
            target=start_https_server,
            args=(build_dir, host_ip, port)
        )
        
        server_process.daemon = True
        server_process.start()
        time.sleep(3)  # Let server start
        
        # Step 4: Reset device and provide OTA URL via stdin (ESP-IDF pattern)
        dut.serial.hard_reset()
        
        # Wait for device to boot and connect to WiFi/Ethernet
        dut.expect('Initialising WiFi Connection...', timeout=60)
        dut.expect('Connected to', timeout=60)
        dut.expect('Returned from app_main()', timeout=10)
        # Provide the URL via stdin (must include newline for fgets)
        print(f'Providing OTA URL to device: {ota_url}')
        dut.write(f'{ota_url}\n')
        
        dut.expect('Rebooting in 5 seconds...', timeout=60)
        
        # Step 5: Wait for reboot and verify new firmware
        # After OTA, the new firmware should log "OTA performed successfully"
        dut.expect('OTA performed successfully', timeout=60)
        
        # Cleanup
        server_process.terminate()
        server_process.join(timeout=5)
        if server_process.is_alive():
            server_process.kill()
        
    except Exception as e:
        print(f'{protocol.upper()} Delta OTA test FAILED: {str(e)}')
        raise

@pytest.mark.parametrize('target', ['esp32'])
@pytest.mark.ethernet
def test_delta_ota_https(dut: Dut):
    """
    Test delta OTA over HTTPS.
    
    This test follows ESP-IDF's simple_ota_example pattern:
    - Uses pre-built binaries from build_esp*_base and build_esp*_new directories
      (created by CI using sdkconfig.ci.base and sdkconfig.ci.new)
    - Base binary is flashed (without EXAMPLE_TEST_DELTA_OTA)
    - New binary has EXAMPLE_TEST_DELTA_OTA enabled
    - Generates delta patch from these pre-built binaries
    - Uses sdkconfig.ci with CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL="FROM_STDIN"
    - Dynamically provides OTA URL via device stdin
    - Verifies successful delta OTA update by checking for "OTA performed successfully" message
    """
    _delta_ota_common(dut, protocol='https', port=8443)

