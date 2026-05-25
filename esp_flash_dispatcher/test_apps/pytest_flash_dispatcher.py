# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize

@pytest.mark.quad_psram
@idf_parametrize('target', [ 'esp32s3'], indirect=['target'])
def test_esp_flash_dispatcher(dut) -> None:
    dut.run_all_single_board_cases()


@pytest.mark.generic
@idf_parametrize('target', ['esp32', 'esp32h4', 'esp32p4', 'esp32s31', 'esp32c61'], indirect=['target'])
def test_esp_flash_dispatcher_generic(dut) -> None:
    dut.run_all_single_board_cases()


@pytest.mark.generic
@idf_parametrize('target', ['esp32c5'], indirect=['target'])
@pytest.mark.parametrize(
    'config',
    [
        'c5_120m_120m_160m',
    ],
    indirect=True,
)
def test_esp_flash_dispatcher_c5(dut) -> None:
    dut.run_all_single_board_cases()
