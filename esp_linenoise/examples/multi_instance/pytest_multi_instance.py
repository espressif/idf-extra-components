# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize
import glob
from pathlib import Path


@pytest.mark.generic
@pytest.mark.skipif(
    not bool(glob.glob(f'{Path(__file__).parent.absolute()}/build*/')),
    reason="Skip the idf version that did not build"
)
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_examples_multi_instance(dut: Dut) -> None:
    dut.expect_exact("Current mode: user", timeout=10)
    dut.write("test")
    dut.expect_exact("Current mode: user", timeout=10)
    dut.write("switch")
    dut.expect_exact("Current mode: admin", timeout=10)
    dut.write("test")
    dut.expect_exact("Current mode: admin", timeout=10)
    dut.write("exit")
    dut.expect_exact("end of example", timeout=10)
