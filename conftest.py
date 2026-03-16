from _pytest.fixtures import FixtureRequest
from pytest_embedded.plugin import multi_dut_argument, multi_dut_fixture
import pytest
import os
import logging
import typing as t

@pytest.hookimpl(tryfirst=True)  # run early
def pytest_ignore_collect(collection_path, config):
    ignoring = config.getoption("ignore") or []
    for pattern in ignoring:
        if collection_path.match(pattern) or str(collection_path).startswith(pattern):
            print(f"pytest would ignore: {collection_path}")
            return True

    for glob_pattern in config.getoption("ignore_glob") or []:
        if collection_path.match(glob_pattern):
            print(f"pytest would ignore by glob: {collection_path}")
            return True

    return False

@pytest.fixture
@multi_dut_argument
def config(request: FixtureRequest) -> str:
    """Fixture that provides the configuration for tests.

    :param request: Pytest fixture request

    :returns: Configuration string, defaults to 'default' if not specified
    """
    return getattr(request, 'param', None) or 'default'



@pytest.fixture
@multi_dut_fixture
def build_dir(
    request: FixtureRequest,
    app_path: str,
    target: t.Optional[str],
    config: t.Optional[str],
) -> str:
    """Find a valid build directory based on priority rules.

    Checks local build directories in the following order:

    1. build_<target>_<config>
    2. build_<target>
    3. build_<config>
    4. build

    :param request: Pytest fixture request
    :param app_path: Path to the application
    :param target: Target being used
    :param config: Configuration being used

    :returns: Valid build directory name, or skips the test if no build directory is found
    """
    check_dirs = []
    build_dir_arg = request.config.getoption('build_dir')

    if build_dir_arg:
        check_dirs.append(build_dir_arg)
    if target is not None and config is not None:
        check_dirs.append(f'build_{target}_{config}')
    if target is not None:
        check_dirs.append(f'build_{target}')
    if config is not None:
        check_dirs.append(f'build_{config}')
    check_dirs.append('build')

    for check_dir in check_dirs:
        binary_path = os.path.join(app_path, check_dir)
        if os.path.isdir(binary_path):
            logging.info(f'Found valid binary path: {binary_path}')
            return check_dir

        logging.warning('Checking binary path: %s... missing... trying another location', binary_path)

    pytest.skip(
        f'No valid build directory found (checked: {", ".join(check_dirs)}). '
        f'Build the binary via "idf.py -B {check_dirs[0]} build" to enable this test.'
    )
