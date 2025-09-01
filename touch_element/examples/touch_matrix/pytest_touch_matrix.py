# SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut
import glob
from pathlib import Path


@pytest.mark.generic
@pytest.mark.skipif(
    not bool(glob.glob(f'{Path(__file__).parent.absolute()}/build*/')),
    reason="Skip the idf version that not build"
)
def test_touch_matrix(dut: Dut) -> None:
    dut.expect_exact('Touch Matrix Example: Touch element library installed')
    dut.expect_exact('Touch Matrix Example: Touch matrix installed')
    dut.expect_exact('Touch Matrix Example: Touch matrix created')
    dut.expect_exact('Touch Matrix Example: Touch element library start')
