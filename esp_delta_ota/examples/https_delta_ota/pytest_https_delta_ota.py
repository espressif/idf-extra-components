# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import contextlib
import os
import subprocess
import sys
import pexpect
from typing import Any

import esptool
import pytest
from pytest_embedded import Dut

PATCH_DATA_PARTITION_OFFSET = 0x2A0000 # Hardcoded offset for patch_data partition from partitions.csv

def get_env_config_variable(env_name, var_name):
    return os.environ.get(f'{env_name}_{var_name}'.upper())

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
    return ip_address


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

def write_patch_to_partition(dut: Dut, patch_file: str):
    """Write the patch file to the patch_data partition on the device.

    Uses the existing esptool connection managed by pytest-embedded to avoid
    serial port conflicts. The device is hard-reset after writing so it
    boots fresh with the patch available on the partition.
    """
    patch_size = os.path.getsize(patch_file)

    # Hardcoded offset for patch_data partition from partitions.csv
    # OTA partitions must be aligned to 0x10000 boundaries
    # Calculated as: phy_init ends at 0x11000, ota_0 aligned to 0x20000, ota_1 at 0x160000, patch_data at 0x2A0000
    offset = PATCH_DATA_PARTITION_OFFSET
    print(f'Writing patch ({patch_size} bytes) to patch_data partition at offset {hex(offset)}')

    serial = dut.serial

    # Reuse the same pattern as EspSerial.use_esptool() decorator:
    #   1. stop the serial redirect thread (releases the pyserial port)
    #   2. let esptool reuse the existing connection
    #   3. resume the redirect thread when done
    with serial.disable_redirect_thread():
        with contextlib.redirect_stdout(serial._q):
            settings = serial.proc.get_settings() # Save the current serial settings
            serial.esp.connect() # Connect to the device using esptool
            esptool.main(
                ['write-flash', hex(offset), patch_file],
                esp=serial.esp,
            )
            serial.proc.apply_settings(settings) # Restore the original serial settings

    print('Successfully wrote patch to patch_data partition')

    # Hard-reset so the device boots fresh (network + local server + stdin wait)
    serial.hard_reset()


@pytest.mark.parametrize('target', ['esp32'])
@pytest.mark.ethernet
def test_esp_delta_ota(dut: Dut):
    example_dir = os.path.dirname(os.path.abspath(__file__))
    build_dir = dut.app.binary_path
    chip_target = getattr(dut, 'target', None) or os.environ.get('IDF_TARGET', 'esp32')
    
    try:
        # Step 1: Get base and new binaries
        base_binary = os.path.join(build_dir, 'https_delta_ota.bin')
        if not os.path.exists(base_binary):
            raise Exception(f'Base binary not found at {base_binary}. Device was flashed from build directory: {build_dir}')

        binary_name = f'hello_world_{chip_target}.bin'
        new_binary = os.path.join(example_dir, 'main', 'tests', binary_name)

        if not os.path.exists(new_binary):
            raise Exception(f'New binary not found at {new_binary}. Expected pre-built binary: {binary_name} in tests/ directory. Example dir: {example_dir}')

        # Step 2: Generate patch
        patch_file = os.path.join(build_dir, 'patch.bin')
        generate_patch(base_binary, new_binary, patch_file, chip_target)

        # Step 3: Write patch to the patch_data partition
        write_patch_to_partition(dut, patch_file)
        
        # Step 4: Connect device and get IP
        env_name = 'wifi_high_traffic' if dut.app.sdkconfig.get('EXAMPLE_WIFI_SSID_PWD_FROM_STDIN') is True else None
        device_ip = setting_connection(dut, env_name)
        print(f'Device connected with IP: {device_ip}')
        
        # Step 5: Wait for local server to start
        dut.expect('Local HTTPS server started for CI test', timeout=30)
        print('Local HTTPS server started on device')

        # Step 6: Provide OTA URL to device (using device's own IP)
        patch_size = os.path.getsize(patch_file)
        ota_url = f'https://{device_ip}:443/patch.bin'
        print(f'Providing OTA URL to device: {ota_url} {patch_size}')
        dut.expect('Reading OTA URL from stdin', timeout=60)
        dut.write(f'{ota_url} {patch_size}\n')

        # Step 7: Wait for OTA to start and complete
        dut.expect('Rebooting in', timeout=90)  # Device preparing to reboot
        
        # Step 8: Wait for reboot and new firmware to boot
        dut.expect('Hello world!', timeout=60)
        
        print('Delta OTA test PASSED: Successfully updated from https_delta_ota to hello_world')
        
    except Exception as e:
        print(f'HTTPS Delta OTA test FAILED: {str(e)}')
        raise
