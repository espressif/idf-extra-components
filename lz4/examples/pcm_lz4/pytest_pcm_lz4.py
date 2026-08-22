# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32s3', 'esp32c3', 'esp32c5'], indirect=['target'])
def test_pcm_lz4_example(dut: Dut) -> None:
    """Both example paths must complete a verified round trip."""
    dut.expect_exact("Block API example")
    dut.expect(r"source_size=[0-9]+ compressed_size=[0-9]+ verify=true")
    dut.expect_exact("Frame streaming API example")
    dut.expect(r"source_size=[0-9]+ frame_size=[0-9]+ verify=true")
