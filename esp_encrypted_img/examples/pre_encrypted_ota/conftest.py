import logging
import os

import pytest
from _pytest.fixtures import FixtureRequest
from pytest_embedded.plugin import multi_dut_fixture

# @pytest.fixture
# @multi_dut_fixture
# def build_dir(request: FixtureRequest, app_path: str, target: str, config: str) -> str:
#     check_dirs = []
#     build_dir_arg = request.config.getoption('build_dir', None)
#     if build_dir_arg:
#         check_dirs.append(build_dir_arg)
#     if target is not None and config is not None:
#         print(f'build_{target}_{config} i am hererere')
#         check_dirs.append(f'build_{target}_{config}')
#     if target is not None:
#         print(f'build_{target} i am hererere')
#         check_dirs.append(f'build_{target}')
#     if config is not None:
#         print(f'build_{config} i am hererere')
#         check_dirs.append(f'build_{config}')
#     check_dirs.append('build')

#     for check_dir in check_dirs:
#         binary_path = os.path.join(app_path, check_dir)
#         if os.path.isdir(binary_path):
#             logging.info(f'found valid binary path: {binary_path}')
#             return check_dir
#     raise ValueError(f'No valid binary path found in {check_dirs}')


@pytest.fixture
@multi_dut_fixture
def build_dir(target: str, config: str) -> str:
    print(f'build_{target}_{config} i am hererere')
    return f'build_{target}_{config}'

@pytest.fixture
@multi_dut_fixture
def config(request: FixtureRequest) -> str:
    return getattr(request, 'param', None) or "default"