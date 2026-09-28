# SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut

CROP_ROWS = 26
CROP_COLS = 72
SCANS = 10


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32c3'], indirect=['target'])
def test_progressive_jpeg_example(dut: Dut) -> None:
    dut.expect_exact('Progressive JPEG decoding, one chunk at a time')
    dut.expect(r'Progressive image, \d+x\d+, \d+ components, \d+ bytes')

    for scan in range(1, SCANS + 1):
        # One output pass per scan: the picture at that point, then its score.
        dut.expect(r'pass \d+, \d+ of \d+ bytes received')
        for _ in range(CROP_ROWS):
            dut.expect(r'[ .:\-=+*#%@]{%d}' % CROP_COLS)
        dut.expect(r'\^ scan %d of \d+, detail \d+' % scan)

    # Every byte was handed over, and the source really did suspend: with a
    # 64-byte release window a 16 KB file cannot be read without it.
    dut.expect(r'decoded \d+ scans from \d+ bytes, [1-9]\d* suspensions')
    dut.expect_exact('done')
