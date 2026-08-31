# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
import subprocess
import logging


@pytest.fixture(autouse=True, scope='session')
def setup_vcan_interface():
    """Ensure vcan0 interface is available for QEMU CAN testing."""
    created_interface = False
    
    try:
        # Check if vcan0 exists
        result = subprocess.run(['ip', 'link', 'show', 'vcan0'], 
                              capture_output=True, text=True, check=False)
        if result.returncode != 0:
            logging.info("Creating vcan0 interface...")
            # Try creating vcan0 interface
            subprocess.run(['sudo', 'ip', 'link', 'add', 'dev', 'vcan0', 'type', 'vcan'], 
                          capture_output=True, text=True, check=False)
            created_interface = True
        
        # Ensure it's up
        subprocess.run(['sudo', 'ip', 'link', 'set', 'up', 'vcan0'], 
                      capture_output=True, text=True, check=False)
        logging.info("vcan0 interface ready")
        
    except Exception as e:
        logging.warning(f"Could not setup vcan0: {e}. QEMU will handle CAN interface setup.")
    
    yield  # Test execution happens here
    
    # Cleanup: Remove vcan0 interface if we created it
    if created_interface:
        try:
            subprocess.run(['sudo', 'ip', 'link', 'delete', 'vcan0'], 
                          capture_output=True, text=True, check=False)
            logging.info("vcan0 interface cleaned up")
        except Exception as e:
            logging.warning(f"Could not cleanup vcan0: {e}")