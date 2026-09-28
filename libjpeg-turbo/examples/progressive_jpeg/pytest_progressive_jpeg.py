# SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut

WIDTH = 64
HEIGHT = 32
SCANS = 3


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32c3', 'esp32c5'], indirect=['target'])
def test_progressive_jpeg_example(dut: Dut) -> None:
    dut.expect_exact('Progressive JPEG decoding, one chunk at a time')
    dut.expect_exact('Progressive image, 64x32, 3 components, 1131 bytes')

    details = []
    for scan in range(1, SCANS + 1):
        dut.expect(r'pass \d+, \d+ of \d+ bytes received')
        for _ in range(HEIGHT):
            dut.expect(r'[ .:\-=+*#%@]{%d}' % WIDTH)
        match = dut.expect(r'\^ scan %d of \d+, detail (\d+)' % scan)
        details.append(int(match.group(1).decode('utf-8')))

    assert details[-1] > details[0], details

    dut.expect(r'decoded %d scans from \d+ bytes, [1-9][0-9]* suspensions' % SCANS)
    dut.expect_exact('done')
