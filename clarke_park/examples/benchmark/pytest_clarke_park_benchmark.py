# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s31'], indirect=['target'])
def test_clarke_park_benchmark(dut: Dut) -> None:
    dut.expect_exact('clarke_park benchmark finished', timeout=60)
