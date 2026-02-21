# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize

@pytest.mark.host_test
@idf_parametrize('target', ['linux'], indirect=['target'])
def test_esp_ext_part_tables_example_basic_linux(dut: Dut) -> None:
    dut.expect_exact('Starting MBR parsing example task')
    dut.expect_exact('MBR parsing example task completed successfully')
    dut.expect_exact('Starting MBR generation example task')
    dut.expect_exact('MBR generation example task completed successfully')
