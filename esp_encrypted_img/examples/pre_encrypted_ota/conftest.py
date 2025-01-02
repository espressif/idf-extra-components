import logging
import os

import pytest
from _pytest.fixtures import FixtureRequest

@pytest.fixture
def config(request: FixtureRequest) -> str:
    return getattr(request, 'param', None) or 'defaults'