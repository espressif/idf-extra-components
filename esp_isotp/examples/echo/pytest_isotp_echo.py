# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
import time
import can
import isotp
import threading
import logging
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize
from typing import List, Tuple

# Test configuration - match sdkconfig defaults  
ESP32C3_RX_ID = 0x7E0  # PC sends to ESP32C3
ESP32C3_TX_ID = 0x7E8  # PC receives from ESP32C3
DEFAULT_TIMEOUT = 5.0

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

class ISOTPTester:
    """
    A robust tester for ISO-TP echo functionality, designed for clear and maintainable testing.
    This class can be used as a context manager to ensure proper resource handling.
    """

    def __init__(self, dut: Dut, block_size: int = 8, stmin: int = 3, rx_timeout: int = 2000) -> None:
        self.dut = dut
        self.bus = None
        self.isotp_stack = None
        self.running = False
        self.process_thread = None
        self.block_size = block_size
        self.stmin = stmin
        self.rx_timeout = rx_timeout

    def __enter__(self) -> 'ISOTPTester':
        """Enter the runtime context and connect to the CAN bus."""
        try:
            self.bus = can.Bus(interface='socketcan', channel='vcan0')
            address = isotp.Address(txid=ESP32C3_RX_ID, rxid=ESP32C3_TX_ID)
            self.isotp_stack = isotp.CanStack(
                bus=self.bus,
                address=address,
                params={'stmin': self.stmin, 'blocksize': self.block_size, 'wftmax': 0, 'rx_flowcontrol_timeout': self.rx_timeout}
            )
            self.running = True
            self.process_thread = threading.Thread(target=self._process, daemon=True)
            self.process_thread.start()
            logging.info(f"Successfully connected to CAN bus with BS={self.block_size}, STmin={self.stmin}")
        except Exception as e:
            logging.error(f"Failed to connect to CAN bus: {e}")
            self.close()
            raise
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """Exit the runtime context and clean up resources."""
        self.close()

    def _process(self) -> None:
        """Background thread for processing ISO-TP stack events."""
        while self.running:
            if self.isotp_stack:
                self.isotp_stack.process()
            time.sleep(0.001)

    def test_echo(self, data: bytes, test_name: str, timeout: float = DEFAULT_TIMEOUT) -> bool:
        """
        Sends data and verifies that the received echo matches the original data.

        Args:
            data: The byte string to be sent.
            test_name: A descriptive name for the test case.
            timeout: The maximum time to wait for the echo.

        Returns:
            True if the test passes, False otherwise.
        """
        if not self.isotp_stack:
            logging.error(f"{test_name}: ISO-TP stack not available.")
            return False

        frame_type = 'SF' if len(data) <= 7 else 'MF'
        logging.info(f"Running test: {test_name} ({len(data)} bytes, {frame_type})")

        try:
            send_start = time.time()
            self.isotp_stack.send(data)
            send_duration = time.time() - send_start

            recv_start = time.time()
            while time.time() - recv_start < timeout:
                if self.isotp_stack.available():
                    received = self.isotp_stack.recv()
                    recv_duration = time.time() - recv_start

                    if received == data:
                        logging.info(f"'{test_name}' PASSED (send: {send_duration:.3f}s, recv: {recv_duration:.3f}s)")
                        return True
                    else:
                        logging.error(f"'{test_name}' FAILED: Data mismatch. Got {len(received)} bytes, expected {len(data)}.")
                        return False
                time.sleep(0.01)

            logging.error(f"'{test_name}' FAILED: Timeout after {timeout}s.")
            return False

        except Exception as e:
            logging.error(f"'{test_name}' FAILED with exception: {e}")
            return False

    def close(self) -> None:
        """Shuts down the CAN bus connection and stops the processing thread."""
        self.running = False
        if self.process_thread and self.process_thread.is_alive():
            self.process_thread.join(timeout=1.0)
        if self.bus:
            self.bus.shutdown()
        logging.info("Connection closed and resources released.")

# Test data for basic and robustness tests
ALL_TEST_CASES = {
    'basic': [
        # Single Frame (SF) tests
        (b'\x01', 'SF-1: 1 byte'),
        (b'\x01\x02\x03\x04', 'SF-4: 4 bytes'),
        (b'\x01\x02\x03\x04\x05\x06\x07', 'SF-7: Max SF'),

        # Multi-Frame (MF) tests
        (bytes(range(8)), 'MF-8: Min MF'),
        (bytes(range(16)), 'MF-16: 2 CF'),

        # Pattern tests
        (b'\x00' * 8, 'MF-Pattern: All 0x00'),
        (b'\xAA' * 32, 'MF-Pattern: All 0xAA'),
    ],
    'robustness': [
        (b'\x00', 'Single null byte'),
        (bytes(range(16)), 'Sequential pattern'),
        (b'\xAA\x55' * 8, 'Alternating pattern'),
        (b'\x00\xFF' * 8, 'Mixed pattern'),
        (b'\x11\x22\x33\x44' * 4, 'Repeated pattern'),
    ]
}

@pytest.fixture(scope="function", autouse=True)
def ensure_dut_ready(dut: Dut) -> None:
    """Auto-fixture to ensure the DUT is ready before running tests."""
    dut.expect("ISO-TP Echo Demo started", timeout=30)
    # Wait a bit for system to fully initialize
    time.sleep(2)
    logging.info("DUT started and ISO-TP echo system initialized.")

def run_test_batch(dut: Dut, test_cases: List[Tuple[bytes, str]]) -> None:
    """Helper function to run a batch of test cases."""
    with ISOTPTester(dut) as tester:
        for data, test_name in test_cases:
            assert tester.test_echo(data, test_name), f"Test '{test_name}' failed"
            time.sleep(0.2)  # Brief pause between tests

@pytest.mark.qemu
@pytest.mark.parametrize("category", ['basic', 'robustness'])
@idf_parametrize('target', ['esp32c3'], indirect=['target'])
def test_echo_basic_and_robustness(dut: Dut, target: str, category: str) -> None:
    """
    Parameterized test for basic echo functionality and robustness edge cases.
    """
    test_cases = ALL_TEST_CASES[category]
    run_test_batch(dut, test_cases)

@pytest.mark.qemu
@idf_parametrize('target', ['esp32c3'], indirect=['target'])
def test_echo_flow_control(dut: Dut, target: str) -> None:
    """
    Tests different flow control (FC) parameters (Block Size and STmin).
    This validates the ISO-TP protocol compliance under various FC conditions.
    """
    flow_control_configs = [
        {'block_size': 4, 'stmin': 0, 'description': 'block size 4'},
        {'block_size': 8, 'stmin': 2, 'description': 'With STmin delay'},
    ]
    test_data = bytes(range(64))  # Use a consistent multi-frame payload

    with ISOTPTester(dut) as tester:
        for config in flow_control_configs:
            logging.info(f"Testing Flow Control: {config['description']}")
            address = isotp.Address(txid=ESP32C3_RX_ID, rxid=ESP32C3_TX_ID)
            tester.isotp_stack = isotp.CanStack(
                bus=tester.bus,
                address=address,
                params={'stmin': config['stmin'], 'blocksize': config['block_size'], 'wftmax': 0}
            )
            test_name = f"FC-{config['description']}"
            assert tester.test_echo(test_data, test_name), f"Flow control test '{test_name}' failed"

@pytest.mark.qemu
@idf_parametrize('target', ['esp32c3'], indirect=['target'])
def test_echo_long_data(dut: Dut, target: str) -> None:
    """
    Tests long data transmission with 20 iterations of 2K chunks.
    Total 60KB data simulating firmware transmission.
    """
    chunk_size = 2048
    iterations = 30  # 60KB total
    test_data = bytes(range(256)) * 8  # 2K pattern data

    logging.info(f"Starting long data test: {iterations} x {chunk_size} bytes = {iterations * chunk_size / 1024:.1f}KB")

    with ISOTPTester(dut) as tester:
        start_time = time.time()
        for i in range(iterations):
            test_name = f"Long-data-{i+1}/{iterations}"
            assert tester.test_echo(test_data, test_name, timeout=10.0), f"Iteration {i+1} failed"

            # Brief pause and progress report every 10 chunks
            time.sleep(0.2)  # Small delay between chunks
            if (i + 1) % 10 == 0:
                elapsed = time.time() - start_time
                progress = (i + 1) / iterations * 100
                speed = (i + 1) * chunk_size / elapsed / 1024  # KB/s
                logging.info(f"Progress: {progress:.1f}% ({i+1}/{iterations}) - Speed: {speed:.1f} KB/s")

        total_time = time.time() - start_time
        total_size = iterations * chunk_size / 1024  # KB
        avg_speed = total_size / total_time  # KB/s
        logging.info(f"Long data test completed: {total_size:.1f}KB in {total_time:.1f}s - Speed: {avg_speed:.1f} KB/s")

