# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest

def test_esp_schedule(dut) -> None:
    dut.run_all_single_board_cases()