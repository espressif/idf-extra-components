#!/usr/bin/env python3
"""Plan test jobs from successful build results and manifest permissions."""

import argparse
import hashlib
import json
import os
from pathlib import Path


def collect_test_groups(eligible_apps: set) -> list:
    # Use the same API as `idf-ci test collect`, retaining node IDs as an array.
    # Its GitHub formatter joins IDs with spaces, losing parameter IDs containing spaces.
    from idf_ci import get_pytest_cases

    collection_args = ['--suppress-no-test-exit-code']
    cases = get_pytest_cases(marker_expr='', additional_args=collection_args)
    # idf-ci adds an embedded_services parametrization specifically for --target
    # linux. Collect those IDs in the same context as the Linux worker.
    if any(case.targets == ['linux'] for case in cases):
        cases = [case for case in cases if case.targets != ['linux']] + get_pytest_cases(
            target='linux', marker_expr='', additional_args=collection_args,
        )
    groups = {}
    for case in cases:
        if not case.apps or any(
            (str(Path(app.path).resolve()), app.target, app.config) not in eligible_apps for app in case.apps
        ):
            print(f'Excluding {case.item.nodeid}: application/configuration not successfully built or testing disabled by manifest')
            continue
        markers = set(case.env_markers)
        if case.is_host_test and not case.emulator_marker:
            markers.add('host_test')
        key = (tuple(case.targets), tuple(sorted(markers)), tuple(case.runner_tags))
        group = groups.setdefault(key, {
            'targets': list(key[0]), 'markers': list(key[1]),
            'runner_tags': list(key[2]), 'nodes': [],
        })
        group['nodes'].append(case.item.nodeid)
    return [{**group, 'nodes': sorted(set(group['nodes']))} for _, group in sorted(groups.items())]


def resolve_runner(group: dict):
    """Translate pytest requirements into labels on the shared runner fleet."""
    target = group['targets'][0]
    markers = set(group['markers'])
    tags = set(group['runner_tags']) - {'self-hosted', 'host_test', 'generic'}
    labels = ['self-hosted', 'linux', 'docker']
    pytest_args = ''
    qemu = 'qemu' in markers
    # Emulator selection takes precedence: idf-ci also marks emulator cases host_test.
    if qemu:
        # Preserve the targets supported by the existing QEMU worker setup.
        if target not in {'esp32s3', 'esp32c3'}:
            return None
        tags -= {target, 'qemu'}
        pytest_args = '--embedded-services idf,qemu'
    elif target == 'linux':
        labels = ['ubuntu-latest']
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


def generate_test_matrix(test_groups):
    groups = {}
    for group in test_groups:
        if not group['nodes']:
            continue
        if len(group['targets']) != 1:
            raise ValueError(f'Multi-DUT test groups are not supported by the single-target worker: {group["targets"]}')
        runner = resolve_runner(group)
        if runner is None:
            print(f'Skipping unavailable execution environment: {group["targets"]} {group["markers"]}')
            continue
        identity = json.dumps([group['targets'], sorted(group['markers']), sorted(group['runner_tags'])])
        group_id = hashlib.sha256(identity.encode()).hexdigest()[:16]
        entry = groups.setdefault(group_id, {
            'id': group_id,
            'idf_target': group['targets'][0],
            'marker': ' and '.join(sorted(group['markers'])) or 'unmarked',
            'markers': sorted(group['markers']),
            **runner,
            'nodes': [],
        })
        entry['nodes'] = sorted(set(entry['nodes']) | set(group['nodes']))
    return {'include': [entry for _, entry in sorted(groups.items())]}


def successful_apps(info_files):
    """Match exact build variants; a sibling configuration cannot enable a test."""
    return {
        (str(Path(app['app_dir']).resolve()), app['target'], app['config_name'] or 'default')
        for info_file in info_files
        for line in info_file.read_text().splitlines() if line.strip()
        for app in [json.loads(line)]
        if app['build_status'] == 'success'
    }


def collect_eligible_apps(built_apps):
    from idf_ci.build_collect.scripts import collect_build_apps, enabled_test_targets

    apps, _ = collect_build_apps(paths=['.'], include_only_enabled=True)
    return {
        (str(Path(app.app_dir).resolve()), app.target, app.config_name or 'default')
        for app in apps.values()
        if (str(Path(app.app_dir).resolve()), app.target, app.config_name or 'default') in built_apps
        and app.target in enabled_test_targets(app)
    }


def plan_tests(info_files):
    built = successful_apps(info_files)
    if not built:
        return {'include': []}
    eligible = collect_eligible_apps(built)
    if not eligible:
        return {'include': []}
    return generate_test_matrix(collect_test_groups(eligible))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build_info_dir', type=Path)
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
    print(json.dumps(matrix, indent=2))
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        output.write(f'test_matrix={json.dumps(matrix)}\n')
        output.write(f'has_tests={str(bool(matrix["include"])).lower()}\n')


if __name__ == '__main__':
    main()
