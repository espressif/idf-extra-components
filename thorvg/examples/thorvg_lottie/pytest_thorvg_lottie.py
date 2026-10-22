# SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import pytest
from pytest_embedded import Dut


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32s3'], indirect=['target'])
def test_thorvg_lottie(dut: Dut) -> None:
    dut.expect('Partition size', timeout=60)
