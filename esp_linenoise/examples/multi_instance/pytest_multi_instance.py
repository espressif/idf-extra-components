# SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_examples_multi_instance(dut: IdfDut) -> None:
    dut.expect_exact("Current mode: user", timeout=10)
    dut.write("test")
    dut.expect_exact("Current mode: user", timeout=10)
    dut.write("switch")
    dut.expect_exact("Current mode: admin", timeout=10)
    dut.write("test")
    dut.expect_exact("Current mode: admin", timeout=10)
    dut.write("exit")
    dut.expect_exact("end of example", timeout=10)
