# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest


@pytest.fixture
def config(request):
    """Fixture that provides the configuration name for tests.

    This fixture returns the config parameter value (e.g., 'partial_download')
    which corresponds to sdkconfig.ci.<config> files.
    """
    return getattr(request, 'param', 'default')
