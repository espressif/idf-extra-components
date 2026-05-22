# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import pytest


@pytest.mark.linux
@pytest.mark.host_test
def test_libsrtp2_linux_host(dut):
    dut.expect("libsrtp2 host_test: PASS", timeout=30)
