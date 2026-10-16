# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import glob
from pathlib import Path

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@pytest.mark.skipif(
    not bool(glob.glob(f'{Path(__file__).parent.absolute()}/build*/')),
    reason='Skip the idf version that did not build',
)
@idf_parametrize('target', ['esp32', 'esp32c3', 'esp32c6', 'esp32s3', 'esp32h2'], indirect=['target'])
def test_blemesh_manager(dut: Dut) -> None:
    dut.run_all_single_board_cases()
