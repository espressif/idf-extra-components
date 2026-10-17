#!/usr/bin/env python3
"""Plan test jobs from successful build results and manifest permissions."""

import argparse
import hashlib
import json
import os
from pathlib import Path


def app_key(path, target, config):
    return str(Path(path).resolve()), target, config or 'default'


def collect_test_cases(eligible_apps):
    """Select cases only when every required app/target/config is eligible."""
    from idf_ci import get_pytest_cases

    if not eligible_apps:
        return []
    # Collect all targets so unsupported multi-DUT cases reach worker validation.
    # Collecting each --target separately would silently deselect those cases.
    collection_args = ['--suppress-no-test-exit-code']
    cases = get_pytest_cases(marker_expr='', additional_args=collection_args)
    if any(target == 'linux' for _, target, _ in eligible_apps):
        # idf-ci adds an embedded_services parameter with --target linux.
        # Use those exact runtime IDs, replacing the generic Linux collection.
        cases = [case for case in cases if case.targets != ['linux']] + get_pytest_cases(
            target='linux', marker_expr='', additional_args=collection_args,
        )
    selected = []
    for case in cases:
        if case.apps and all(app_key(app.path, app.target, app.config) in eligible_apps for app in case.apps):
            selected.append(case)
        else:
            print(f'Excluding {case.item.nodeid}: application/configuration not successfully built or testing disabled by manifest')
    return selected


def resolve_runner(target, markers, runner_tags):
    """Translate pytest requirements into labels on the shared runner fleet."""
    tags = set(runner_tags) - {'self-hosted', 'host_test', 'generic'}
    labels = ['self-hosted', 'linux', 'docker']
    pytest_args = ''
    qemu = 'qemu' in markers
    # Emulator selection takes precedence: idf-ci also marks emulator cases host_test.
    if qemu:
        # Preserve the targets supported by the existing QEMU worker setup.
        if target not in {'esp32s3', 'esp32c3'}:
            raise ValueError(f'The QEMU worker does not support target {target}')
        tags -= {target, 'qemu'}
        pytest_args = '--embedded-services idf,qemu'
    elif target == 'linux':
        labels = ['ubuntu-24.04']
        tags.discard(target)
        pytest_args = '--embedded-services idf'
    elif target == 'esp32':
        # These existing stations use legacy labels rather than target + marker.
        # Replace only their tags, preserving additional requirements such as PSRAM.
        for marker, label in {'ethernet': 'ESP32-ETHERNET-KIT', 'spi_nand_flash': 'spi_nand_flash'}.items():
            if marker in markers:
                tags -= {target, marker}
                tags.add(label)
    return {
        'runner_labels': list(dict.fromkeys([*labels, *sorted(tags)])),
        'pytest_args': pytest_args,
        'setup_qemu': qemu,
    }


def generate_test_matrix(groups):
    """Translate idf-ci groups into jobs for the existing GitHub runner fleet."""
    jobs = []
    for cases in groups:
        case = cases[0]
        if len(case.targets) != 1:
            raise ValueError(f'Multi-DUT tests are not supported by the single-target worker: {case.item.nodeid}')
        target = case.targets[0]
        markers = set(case.env_markers)
        if case.is_host_test and not case.emulator_marker:
            markers.add('host_test')
        markers = sorted(markers)
        # Stable artifact IDs also distinguish cases with extra runner tags.
        identity = json.dumps([case.targets, markers, sorted(case.runner_tags)])
        group_id = hashlib.sha256(identity.encode()).hexdigest()[:16]
        jobs.append({
            'id': group_id,
            'idf_target': target,
            'marker': ' and '.join(markers) or 'unmarked',
            'markers': markers,
            **resolve_runner(target, markers, case.runner_tags),
            'nodes': sorted({case.item.nodeid for case in cases}),
        })
    return {'include': sorted(jobs, key=lambda job: job['id'])}


def successful_apps(info_files):
    """A skipped/reconfigured app or sibling config is not a successful build."""
    built = set()
    for info_file in info_files:
        for line in info_file.read_text().splitlines():
            if not line.strip():
                continue
            app = json.loads(line)
            if app['build_status'] == 'build success':
                built.add(app_key(app['app_dir'], app['target'], app['config_name']))
    return built


def collect_eligible_apps(built_apps):
    from idf_ci.build_collect.scripts import collect_build_apps, enabled_test_targets

    apps, _ = collect_build_apps(paths=['.'], include_only_enabled=True)
    return {
        app_key(app.app_dir, app.target, app.config_name)
        for app in apps.values()
        if app_key(app.app_dir, app.target, app.config_name) in built_apps
        and app.target in enabled_test_targets(app)
    }


def plan_tests(info_files):
    built = successful_apps(info_files)
    if not built:
        return {'include': []}
    from idf_ci.idf_pytest import GroupedPytestCases

    eligible = collect_eligible_apps(built)
    groups = GroupedPytestCases(collect_test_cases(eligible)).grouped_cases
    return generate_test_matrix(groups.values())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build_info_dir', type=Path)
    parser.add_argument('--selection-report', type=Path, default=Path('test-selection.json'))
    args = parser.parse_args()
    info_files = sorted(args.build_info_dir.glob('build_info_*.json'))
    # Never silently interpret a missing shard artifact as an empty build result.
    expected = {
        f'build_info_{job["idf_target"]}_{job["parallel_index"]}.json'
        for job in json.loads(os.environ['BUILD_MATRIX'])['include']
    }
    actual = {path.name for path in info_files}
    if actual != expected:
        raise SystemExit(f'Build metadata mismatch: missing={sorted(expected - actual)}, unexpected={sorted(actual - expected)}')
    matrix = plan_tests(info_files)
    args.selection_report.write_text(json.dumps(matrix, indent=2) + '\n')
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as summary:
            summary.write(f'### Test selection\n\n{len(matrix["include"])} groups, '
                          f'{sum(len(g["nodes"]) for g in matrix["include"])} pytest variants.\n\n'
                          'See the test_selection artifact for exact node IDs.\n')
    print(json.dumps(matrix, indent=2))
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        output.write(f'test_matrix={json.dumps(matrix)}\n')
        output.write(f'has_tests={str(bool(matrix["include"])).lower()}\n')


if __name__ == '__main__':
    main()
