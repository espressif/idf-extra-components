# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32c3'], indirect=['target'])
def test_hello_jpeg_example(dut: Dut) -> None:
    dut.expect_exact('app_main started')

    # First asset: image.jpg (320x240), decoded to a PPM header then discarded.
    dut.expect_exact('P6')
    dut.expect_exact('320 240')
    dut.expect_exact('255')
    dut.expect_exact('jpeg_start_decompress')
    dut.expect(r'jpeg_finish_decompress, time = \d+')
    dut.expect_exact('jpeg_destroy_decompress')

    # Second asset: image32x32.jpg, printed as 32 columns of ASCII.
    dut.expect_exact('P6')
    dut.expect_exact('32 32')
    dut.expect_exact('255')
    dut.expect_exact('Decoded image 32x32:')
    for _ in range(32):
        dut.expect(r'[ #.+]{32}')
    dut.expect_exact('done')
