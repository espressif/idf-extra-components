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