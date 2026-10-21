#!/usr/bin/env python3
"""Audit exact app/target/config coverage and render the native idf-ci report."""

import argparse
import html
import json
import os
from pathlib import Path


def app_key(path, target, config):
    return str(Path(path).resolve()), target, config or 'default'


def report_errors(errors):
    for error in errors:
        message = (f'{error["app"]} (target={error["target"]}, config={error["config"]}): '
                   f'no build definition for {", ".join(error["tests"])}')
        # Escape workflow-command data so paths and node IDs remain literal.
        message = message.replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')
        print(f'::error title=Tests without build definitions::{message}')


def classify(projects, buildable, build_only, sdk_targets):
    """Several cases may cover one variant; disabled cases also explain coverage."""
    result = {key: [] for key in ('tested', 'disabled', 'build_only', 'unsupported', 'warnings', 'errors')}
    for path, project in projects.items():
        # Building every chip is intentional compile coverage here; a variant
        # only looks suspicious when the same app IS tested on that target
        # with another config, but this one has no case at all.
        tested_targets = {app['target'] for app in project['apps']
                          if any(not case.get('disabled', False) for case in app['test_cases'])}
        for app in project['apps']:
            key = app_key(path, app['target'], app['config'])
            if key not in buildable:
                continue
            row = dict(app=path, target=app['target'], config=app['config'] or 'default')
            cases = app['test_cases']
            if any(not case.get('disabled', False) for case in cases):
                result['tested'].append(row)
            elif cases:
                result['disabled'].append({**row, 'reasons': sorted({case.get('skip_reason') or case.get('test_comment', '') for case in cases})})
            elif key in build_only:
                result['build_only'].append({**row, 'reason': build_only[key]})
            elif app['target'] in tested_targets:
                result['warnings'].append(row)
            else:
                result['build_only'].append({**row, 'reason': 'no test case declares this variant'})
        for app in project['missing_apps']:
            cases = [case for case in app['test_cases'] if not case.get('disabled', False)]
            if cases:
                row = dict(app=path, target=app['target'], config=app['config'] or 'default',
                           tests=[case['caseid'] for case in cases])
                if app['target'] not in sdk_targets and project['apps']:
                    # A real app may declare tests for a target introduced in a
                    # newer SDK. Discovery cannot define that variant here.
                    result['unsupported'].append({**row, 'reason': 'Target is not available in this ESP-IDF SDK'})
                else:
                    result['errors'].append(row)
    return result


def read_build_only(path, known_apps):
    """Optional, explicit build-only declarations; no test nodes are fabricated."""
    if not path.exists():
        return {}
    from esp_bool_parser import parse_bool_expr

    declarations = json.loads(path.read_text())
    if not isinstance(declarations, list):
        raise ValueError('Build-only declarations must be a JSON array')
    selected = {}
    for rule in declarations:
        if not isinstance(rule, dict) or set(rule) - {'app', 'condition', 'reason'}:
            raise ValueError('Build-only declarations accept app, condition and reason')
        if any(not isinstance(rule.get(key), str) or not rule[key].strip() for key in ('app', 'reason')):
            raise ValueError('Build-only declarations require an app path and a reason')
        app_path = str(Path(rule['app']).resolve())
        matches = [key for key in known_apps if key[0] == app_path]
        if not matches:
            raise ValueError(f'Unknown build-only app: {rule["app"]}')
        expression = rule.get('condition')
        if expression is not None and (not isinstance(expression, str) or not expression.strip()):
            raise ValueError('Build-only condition must be a non-empty IDF boolean expression')
        condition = parse_bool_expr(expression) if expression is not None else None
        for key in matches:
            if condition is None or condition.get_value(key[1], key[2]):
                selected[key] = rule['reason']
    return selected


def collect_inventory():
    from idf_build_apps.constants import BuildStatus
    from idf_ci import get_all_apps
    from idf_ci.build_collect.models import AppKey, CollectResult, ProjectInfo
    from idf_ci.build_collect.scripts import (
        collect_build_apps, collect_test_apps, group_by_path, process_apps, process_missing_apps,
    )

    os.environ['IDF_CI_INCLUDE_DISABLED_TESTS'] = '1'
    # Capture SDK-supported candidates before the report collector expands the
    # manifest's default targets to include preview targets.
    planned = {AppKey(*app_key(app.app_dir, app.target, app.config_name)): app
               for group in get_all_apps(marker_expr='') for app in group
               if app.build_status == BuildStatus.SHOULD_BE_BUILT}
    apps, paths = collect_build_apps()
    # Host apps opt into linux via manifest enable rules; neither discovery
    # above considers linux (same reason as generate_build_matrix).
    from idf_build_apps import find_apps
    from idf_build_apps.constants import SUPPORTED_TARGETS
    for app in find_apps(['.'], 'linux', default_build_targets=list(SUPPORTED_TARGETS)):
        key = AppKey(*app_key(app.app_dir, app.target, app.config_name))
        planned.setdefault(key, app)
        apps.setdefault(key, app)
        paths.setdefault(key.path, app.app_dir)
    # Native collection uses '' for unnamed configs; pytest calls them 'default'.
    apps = {AppKey(key.path, key.target, key.config or 'default'): app for key, app in apps.items()}
    # Use the actual build plan for warnings and include its unnamed defaults.
    for key, app in planned.items():
        apps.setdefault(key, app)
        paths.setdefault(key.path, app.app_dir)
    tests = collect_test_apps()
    app_groups, test_groups = group_by_path(apps), group_by_path(tests)
    inventory = CollectResult()
    for path in sorted(set(app_groups) | set(test_groups)):
        project_apps, project_tests = app_groups.get(path, {}), test_groups.get(path, {})
        info, used, disabled = process_apps(project_apps, project_tests)
        missing, missing_cases = process_missing_apps(project_apps, project_tests)
        inventory.projects[paths.get(path, os.path.relpath(path))] = ProjectInfo(apps=info, missing_apps=missing)
        inventory.summary.total_test_cases_used += len(used)
        inventory.summary.total_test_cases_disabled += len(disabled)
        inventory.summary.total_test_cases_missing_app += len(missing_cases)
        inventory.summary.total_test_cases += len(used | disabled | missing_cases)
    inventory.summary.total_projects = len(inventory.projects)
    inventory.summary.total_apps = sum(app.build_status != BuildStatus.DISABLED for app in apps.values())
    return inventory, set(planned), set(apps)


def main():
    from idf_build_apps.constants import ALL_TARGETS
    from idf_ci.build_collect.scripts import format_as_html

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--build-only', type=Path, default=Path('.github/build-only-apps.json'))
    args = parser.parse_args()
    inventory, buildable, known_apps = collect_inventory()
    # Include preview targets and Linux: missing configs on those must still fail.
    audit = classify(inventory.model_dump()['projects'], buildable, read_build_only(args.build_only, known_apps),
                     set(ALL_TARGETS) | {'linux'})
    summary = ', '.join(f'{name}: {len(rows)}' for name, rows in audit.items())
    report = format_as_html(inventory)
    details = '<section><h2>Build/test coverage</h2><p>' + html.escape(summary) + '</p>'
    details += '<p>Coverage is checked per application, target and configuration. Disabled nodes retain their reasons. Full audit details are in the accompanying JSON file.</p></section>'
    if audit['unsupported']:
        details += '<details><summary>Tests outside this SDK\'s target list</summary><ul>'
        for row in audit['unsupported']:
            details += '<li>' + html.escape(f'{row["app"]} ({row["target"]}, {row["config"]}): {row["reason"]}') + '</li>'
        details += '</ul></details>'
    args.output.write_text(report.replace('<body>', '<body>' + details, 1))
    args.output.with_suffix('.json').write_text(json.dumps(audit, indent=2) + '\n')
    print(summary)
    if audit['warnings']:
        print(f'::warning title=Unexplained build-only variants::{len(audit["warnings"])} build variants have no enabled/disabled tests or explicit build-only declaration. See the coverage report.')
    if audit['errors']:
        report_errors(audit['errors'])
        raise SystemExit(1)


if __name__ == '__main__':
    main()
