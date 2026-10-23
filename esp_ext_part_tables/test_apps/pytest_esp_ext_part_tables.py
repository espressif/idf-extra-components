# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0
import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.host_test
@idf_parametrize('target', ['linux'], indirect=['target'])
def test_esp_ext_part_tables(dut: Dut) -> None:
    dut.run_all_single_board_cases()


# The host build cannot catch target-only problems: pointer width (the host is 64-bit),
# Unity's 64-bit assertion support, and real heap accounting for the leak checks.
# Running the same cases under QEMU covers those without needing hardware.
@pytest.mark.qemu
@idf_parametrize('target', ['esp32s3', 'esp32c3'], indirect=['target'])
def test_esp_ext_part_tables_qemu(dut: Dut) -> None:
    dut.run_all_single_board_cases()
