# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
import os
from pytest_embedded import Dut
import time
try:
    from PIL import Image
    from PIL import ImageChops
except ImportError:
    Image = None

@pytest.mark.qemu
@pytest.mark.parametrize('qemu_extra_args', ['-display sdl'])
@pytest.mark.parametrize('embedded_services', ['idf,qemu'])
def test_qemu_rgb_panel(dut: Dut) -> None:
    if not Image:
        pytest.fail('Pillow is not installed')

    dut.expect_exact('Install RGB LCD panel driver')
    dut.expect_exact('LVGL Scatter Chart displayed')
    time.sleep(2)
    filename = 'scatter_chart.ppm'
    dut.qemu.take_screenshot(filename)

    img = Image.open(filename)
    ref_img = Image.open(os.path.join(os.path.dirname(__file__), 'doc', 'scatter_chart.png'))
    
    diff = ImageChops.difference(img, ref_img)
    assert not diff.getbbox(), 'Images are different'
