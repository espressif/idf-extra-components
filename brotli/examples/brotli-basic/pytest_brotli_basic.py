# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
def test_brotli_basic_example(dut: Dut) -> None:
    dut.expect(r'Compressed \d+ bytes to \d+ bytes')
    dut.expect_exact('Brotli round-trip verified')
