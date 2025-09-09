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
    dut.run_all_single_board_cases(timeout=60)

