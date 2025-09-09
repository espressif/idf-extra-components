#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
def test_esp_daylight(dut: Dut) -> None:
    """
    Test esp_daylight component functionality
    """
    dut.expect_exact('Running esp_daylight component tests')

    # Run all tests
    dut.write('*')

    # Check that all tests pass
    dut.expect('Tests 11 Failures 0', timeout=60)
