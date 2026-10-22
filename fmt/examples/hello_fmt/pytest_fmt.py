# SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut


def _expect_fmt_output(dut: Dut) -> None:
    dut.expect_exact('Hello, fmt!')
    dut.expect_exact('The answer is 42 and pi is approximately 3.142')
    dut.expect_exact('formatted string: \'Hello, ESP-IDF #2!\'')
    dut.expect_exact('written into a fixed buffer: \'idf v6.0\'')
    dut.expect(r'fmt runs on \S+')
    dut.expect_exact('vector: [1, 2, 3, 4, 5]')
    dut.expect_exact('map: {"alice": 90, "bob": 85}')
    dut.expect(r'current time: \d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}')
    dut.expect_exact('one and a half hours: 01:30:00')
    dut.expect_exact('sub-second precision: 1.500s')
    dut.expect_exact('built at run time: dynamic in 2026')
    dut.expect_exact('fmt::sprintf returns a string: \'03.14\'')
    dut.expect_exact('--- done ---')


@pytest.mark.host_test
@pytest.mark.parametrize('target', ['linux'], indirect=['target'])
def test_fmt_example_host(dut: Dut) -> None:
    _expect_fmt_output(dut)


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32s3', 'esp32c5'], indirect=['target'])
def test_fmt_example(dut: Dut) -> None:
    _expect_fmt_output(dut)
