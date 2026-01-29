# SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest


@pytest.fixture
def config(request):
    """
    Fixture that provides the configuration for tests.
    This is a local implementation to support pytest collection on environments
    where idf-ci is not available (e.g., Python 3.14 on Linux test runners).
    When idf-ci is available, its IdfPytestPlugin provides this fixture.
    """
    return getattr(request, 'param', None) or 'default'
