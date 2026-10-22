# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32s3', 'esp32c5'], indirect=['target'])
def test_bdc_motor_example(dut: Dut) -> None:
    dut.expect_exact('Create DC motor')
    dut.expect_exact('Enable motor')
    dut.expect_exact('Set speed to 50%')
    dut.expect_exact('Motor forward')
    dut.expect_exact('Motor reverse')
    dut.expect_exact('Motor coast')
    dut.expect_exact('Motor brake')
