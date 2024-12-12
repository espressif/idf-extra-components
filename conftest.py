# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
# pylint: disable=W0621  # redefined-outer-name
#
# IDF is using [pytest](https://github.com/pytest-dev/pytest) and
# [pytest-embedded plugin](https://github.com/espressif/pytest-embedded) as its test framework.
#
# if you found any bug or have any question,
# please report to https://github.com/espressif/pytest-embedded/issues
# or discuss at https://github.com/espressif/pytest-embedded/discussions
import logging
import os

import pytest
from _pytest.fixtures import FixtureRequest
from pytest_embedded.plugin import multi_dut_fixture

@pytest.fixture
@multi_dut_fixture
def build_dir(target: str, config: str) -> str:
    return f'build_{target}_{config}'

@pytest.fixture
@multi_dut_fixture
def config(request: FixtureRequest) -> str:
    return getattr(request, 'param', None) or "default"
