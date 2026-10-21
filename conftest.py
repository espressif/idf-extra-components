from _pytest.fixtures import FixtureRequest
from pytest_embedded.plugin import multi_dut_fixture
import pytest
import os
import logging
import typing as t


def pytest_collection_modifyitems(config, items):
    """Exclude disabled variants before idf-ci records cases and groups jobs."""
    # Workers execute the frozen post-build plan without an installed ESP-IDF.
    # Re-evaluating SDK/SoC conditions there could change the selected nodes.
    if os.environ.get('TEST_NODES'):
        return
    from idf_ci.idf_pytest.plugin import IdfPytestPlugin

    audit = os.environ.get('IDF_CI_INCLUDE_DISABLED_TESTS') == '1' and config.getoption('collectonly')
    if audit:
        # idf-ci's report already understands this marker and its skip reason.
        config.addinivalue_line('markers', 'temp_skip(targets, reason): disabled node retained for the coverage audit')
    selected, deselected = [], []
    conditions = {}
    for item in items:
        case = IdfPytestPlugin.get_case_by_item(item)
        disabled = False
        reasons = []
        for marker in item.iter_markers('disable_test'):
            reason = marker.kwargs.get('reason')
            if len(marker.args) > 1 or set(marker.kwargs) - {'targets', 'configs', 'reason'}:
                raise pytest.UsageError(f'{item.nodeid}: disable_test accepts one condition plus targets, configs and reason')
            if not isinstance(reason, str) or not reason.strip():
                raise pytest.UsageError(f'{item.nodeid}: disable_test requires a non-empty reason')
            condition = None
            if marker.args:
                expression = marker.args[0]
                if not isinstance(expression, str) or not expression.strip():
                    raise pytest.UsageError(f'{item.nodeid}: disable_test condition must be a non-empty string')
                from esp_bool_parser import parse_bool_expr
                from esp_bool_parser.utils import InvalidIfClause
                from pyparsing import ParseBaseException

                try:
                    if expression not in conditions:
                        conditions[expression] = parse_bool_expr(expression)
                    condition = conditions[expression]
                except (ParseBaseException, InvalidIfClause) as error:
                    raise pytest.UsageError(f'{item.nodeid}: disable_test: {error}') from error
            for key in ('targets', 'configs'):
                if key in marker.kwargs:
                    values = marker.kwargs[key]
                    if not isinstance(values, (list, tuple)) or not values or any(
                        not isinstance(value, str) or not value for value in values
                    ):
                        raise pytest.UsageError(f'{item.nodeid}: disable_test {key} must be a non-empty list of strings')
            targets, configs = marker.kwargs.get('targets'), marker.kwargs.get('configs')
            if (condition is None and targets is None and configs is None) or (case and any(
                (targets is None or app.target in targets) and (configs is None or app.config in configs)
                and (condition is None or condition.get_value(app.target, app.config))
                for app in case.apps
            )):
                disabled = True
                reasons.append(reason)
                logging.info('Deselecting %s: %s', item.nodeid, reason)
        if disabled and audit and case:
            item.add_marker(pytest.mark.temp_skip(targets=case.targets, reason='; '.join(reasons)))
            selected.append(item)
            continue
        (deselected if disabled else selected).append(item)
    items[:] = selected
    if deselected:
        config.hook.pytest_deselected(items=deselected)


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

# config is provided by idf-ci. Keep build_dir's local skip-on-missing behavior;
# the upstream fixture raises an error when no build directory exists.
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
