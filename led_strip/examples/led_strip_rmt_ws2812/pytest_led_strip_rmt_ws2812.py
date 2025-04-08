# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize
from pytest_embedded_idf.utils import soc_filtered_targets

@pytest.mark.generic
@idf_parametrize(
    'target', soc_filtered_targets('SOC_RMT_SUPPORTED == 1'), indirect=['target']
)
def test_led_strip_rmt_ws2812(dut: Dut) -> None:
    dut.expect_exact('example: Created LED strip object with RMT backend')
    dut.expect_exact('example: Start blinking LED strip')
    dut.expect_exact('example: LED OFF!')
    dut.expect_exact('example: LED ON!')

    
