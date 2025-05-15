# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize
from pytest_embedded_idf.utils import soc_filtered_targets

@pytest.mark.generic
@idf_parametrize(
    'target', soc_filtered_targets('SOC_PARLIO_SUPPORTED == 1'), indirect=['target']
)
def test_led_strip_parlio_ws2812(dut: Dut) -> None:
    dut.expect_exact('example: Created LED strip object with PARLIO backend')
    dut.expect_exact('example: Start blinking LED strip')
    dut.expect_exact('example: LED OFF!')
    dut.expect_exact('example: LED ON!')

    
