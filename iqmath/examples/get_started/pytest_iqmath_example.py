# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut


@pytest.mark.disable_test(
    'IDF_TARGET in ["esp32c3", "esp32c5"]',
    reason='Preserve the previous CI runtime coverage; enable additional hardware targets after runner validation',
)
@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32s3', 'esp32c3', 'esp32c5'], indirect=['target'])
def test_iqmath_example(dut: Dut) -> None:
    dut.expect_exact("IQMath test passed")
