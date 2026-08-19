# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import glob
from pathlib import Path

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


# The `generic` marker is what selects this case on CI: the run-target job filters
# with `-m <runner marker>`, so an unmarked test is deselected everywhere and the
# app would only ever be built, never run.
@pytest.mark.generic
@pytest.mark.skipif(
    not bool(glob.glob(f'{Path(__file__).parent.absolute()}/build*/')),
    reason='Skipping: no build directory found for this IDF version'
)
# esp32 is one of the `generic` runners in the run-target matrix; the tests are
# pure date/time logic, so one target is enough coverage.
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_esp_schedule(dut: Dut) -> None:
    dut.run_all_single_board_cases()
