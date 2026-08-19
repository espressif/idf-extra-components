# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
def test_color_emoji_example(dut: Dut) -> None:
    dut.expect(r'Loaded U\+1F642 as \d+x\d+ BGRA bitmap')
    dut.expect(r'FreeType decoded embedded PNG data using libpng; bitmap FNV-1a: [0-9a-f]{8}')
