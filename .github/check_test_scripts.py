#!/usr/bin/env python3
"""Check that pytest target parametrization matches the manifest test targets.

For every app with pytest cases, compares:
  - targets enabled for testing by the component's .build-test-rules.yml
    (App.MANIFEST.enable_test_targets)
  - targets parametrized in the app's pytest scripts (collected via idf-ci)

and fails on mismatch. Ported from esp-idf
tools/ci/check_build_test_rules.py (check-test-scripts action).

Usage: python3 .github/check_test_scripts.py [path ...]
"""

import argparse
import os
import sys
from collections import defaultdict

from idf_build_apps import App
from idf_build_apps import find_apps
from idf_ci import get_pytest_cases

# Targets under bringup: suppress mismatch warnings for these.
BYPASS_TARGETS: list = []


def relpath(path: str) -> str:
    return os.path.normpath(os.path.relpath(path, os.getcwd()))


def get_grouped_apps(paths: list) -> dict:
    grouped_apps = defaultdict(list)
    for app in sorted(find_apps(paths, 'all')):
        grouped_apps[os.path.normpath(app.app_dir)].append(app)
    return grouped_apps


def get_grouped_cases(paths: list) -> dict:
    """
    returns something like this:
    {
        app_dir: {
            'script_paths': {'path/to/script1', 'path/to/script2'},
            'targets': {'esp32', 'esp32s2', ...},
        }
    }
    """
    pytest_cases = get_pytest_cases(
        paths=paths,
        marker_expr=None,  # don't filter host_test
    )

    grouped_cases = {}
    for case in pytest_cases:
        for pytest_app in case.apps:
            app_dir = relpath(pytest_app.path)
            if app_dir not in grouped_cases:
                grouped_cases[app_dir] = {
                    'script_paths': {case.path},
                    'targets': {pytest_app.target},
                }
            else:
                grouped_cases[app_dir]['script_paths'].add(case.path)
                grouped_cases[app_dir]['targets'].add(pytest_app.target)

    return grouped_cases


def check_test_scripts(paths: list) -> int:
    grouped_apps = get_grouped_apps(paths)
    grouped_cases = get_grouped_cases(paths)
    exit_code = 0

    for app_dir, apps in grouped_apps.items():
        if app_dir not in grouped_cases:
            continue

        manifest_targets = sorted(
            {
                target
                for app in apps
                for target in (
                    App.MANIFEST.enable_test_targets(app_dir)
                    + App.MANIFEST.enable_test_targets(app_dir, config_name=app.config_name)
                )
            }
        )
        actual_targets = sorted(grouped_cases[app_dir]['targets'])

        if manifest_targets == actual_targets:
            continue
        if not ((set(manifest_targets) ^ set(actual_targets)) - set(BYPASS_TARGETS)):
            continue

        print(f'Test target MISMATCH!!!: {app_dir}')
        print(f'  Manifest (enable_test_targets): {manifest_targets}')
        print(f'  Test scripts (parametrize):     {actual_targets}')
        print(f'  Corresponding manifest file: {App.MANIFEST.most_suitable_rule(app_dir).by_manifest_file}')
        print('  Corresponding test scripts:')
        for script_path in sorted(grouped_cases[app_dir]['script_paths']):
            print(f'    - {script_path}')
        exit_code = 1

    if exit_code != 0:
        print()
        print('Fix either the manifest rules or the target parametrization in the test script.')
        print('Related documentation:')
        print('  - https://docs.espressif.com/projects/idf-build-apps/en/latest/references/manifest.html#enable-disable-rules')

    return exit_code


if __name__ == '__main__':
    if 'CI_JOB_ID' not in os.environ:
        os.environ['CI_JOB_ID'] = 'fake'  # idf-ci scripts expect a CI environment

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('paths', nargs='*', default=['.'], help='check under paths (default: repo root)')
    args = parser.parse_args()

    sys.exit(check_test_scripts(args.paths))
