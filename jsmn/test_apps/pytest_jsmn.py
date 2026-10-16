# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.disable_test(
    'IDF_TARGET != "esp32"',
    reason='Preserve existing ESP32 runtime coverage; ESP32-C3 retains build coverage until its tests are enabled',
)
@pytest.mark.generic
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_jsmn(dut) -> None:
    dut.run_all_single_board_cases()
