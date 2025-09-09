#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
def test_esp_daylight_example(dut: Dut) -> None:
    """
    Test esp_daylight example application
    """
    # Wait for the example to start
    dut.expect_exact('ESP Daylight Component Example')

    # Check that basic calculations are performed
    dut.expect('Basic Sunrise/Sunset Calculation', timeout=30)
    dut.expect('New York, USA', timeout=10)
    dut.expect('London, UK', timeout=10)
    dut.expect('Pune, India', timeout=10)

    # Check seasonal variations
    dut.expect('Seasonal Variations Example', timeout=10)
    dut.expect('Spring Equinox', timeout=10)
    dut.expect('Summer Solstice', timeout=10)

    # Check time offsets
    dut.expect('Time Offset Example', timeout=10)
    dut.expect('30 minutes before sunset', timeout=10)

    # Check polar regions
    dut.expect('Polar Region Example', timeout=10)
    dut.expect('Midnight Sun', timeout=10)

    # Check practical scheduling
    dut.expect('Practical Scheduling Example', timeout=10)
    dut.expect('Smart Home Lighting Schedule', timeout=10)

    # Wait for completion
    dut.expect('Example completed successfully!', timeout=30)

