# SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_examples_basic_line_reading(dut: IdfDut) -> None:
    message = "test_msg"
    prompt = "esp_linenoise> "
    dut.expect(prompt, timeout=10)
    dut.write(message + '\n')
    dut.expect("end of example", timeout=10)
