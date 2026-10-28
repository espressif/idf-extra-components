# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0

import pexpect
import pytest
from pytest_embedded import Dut


@pytest.mark.disable_test(
    'IDF_TARGET == "esp32p4"',
    reason='Temporarily disable while waiting for an available ESP32-P4 self-hosted runner',
)
@pytest.mark.generic
@pytest.mark.parametrize(
    'port, flash_port',
    [pytest.param('/dev/ttyUSB0', '/dev/ttyUSB0', id='ttyUSB')],
    indirect=True,
)
@pytest.mark.parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_raylib_fireworks(dut: Dut) -> None:
    dut.expect('display 320x240, software renderer, 16 bpp framebuffer', timeout=60)
    dut.expect('Software renderer initialized successfully', timeout=30)

    # Crash and platform-error patterns; matching any of them is a failure,
    # so the timeout that ends this block is the pass condition. Thirty
    # seconds covers dozens of bursts and would trip the task watchdog
    # several times over if the renderer ever stopped blocking.
    with pytest.raises(pexpect.TIMEOUT):
        dut.expect(
            r'Guru Meditation'
            r'|Task watchdog got triggered'
            r'|abort\(\) was called'
            r'|no internal RAM'
            r'|could not start the present task'
            r'|renderer has no color buffer',
            timeout=30,
        )
