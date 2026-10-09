#!/usr/bin/env python3
"""Discover the target/shard matrix in the current ESP-IDF environment.

The caller owns the version matrix. This collector only handles one IDF and
never extrapolates application counts or test availability to other versions.
"""

import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
from collections import Counter


# Shared execution defaults, independent of the caller and ESP-IDF version.
MAX_BUILD_SHARDS = 5
APPS_PER_SHARD = 40


def run_idf_ci(*args: str) -> dict:
    proc = subprocess.run(['idf-ci', *args], capture_output=True, text=True)
    if proc.returncode != 0:
        print(proc.stdout)
        print(proc.stderr, file=sys.stderr)
        raise SystemExit(f'idf-ci {" ".join(args)} failed with exit code {proc.returncode}')
    return json.loads(proc.stdout)


def shard_count(app_count: int, runs_per_job: int, max_shards: int) -> int:
    if app_count <= 0:
        return 0
    return min(math.ceil(app_count / runs_per_job), max_shards)


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
            print(f'Excluding {case.item.nodeid}: application/configuration unavailable or testing disabled by manifest')
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


def generate_matrices(app_counts: Counter, test_groups: list) -> dict:
    """Derive target workers and exact test groups from this SDK's discovery."""
    # Linux builds use a different toolchain and always need their own worker,
    # even when manifests or caller policy disable all host tests.
    tests_by_target = {'linux': {}} if app_counts.get('linux', 0) else {}
    for group in test_groups:
        if not group['nodes']:
            continue
        # The reusable worker builds and restores one target. Never silently split
        # a multi-DUT group into jobs that cannot provide its required devices.
        if len(group['targets']) != 1:
            raise ValueError(f'Multi-DUT test groups are not supported by the single-target worker: {group["targets"]}')
        target = group['targets'][0]
        if not app_counts.get(target, 0):
            continue
        tests_by_target.setdefault(target, {})
        runner = resolve_runner(group)
        if runner is None:
            print(f'Skipping unavailable execution environment: {target} {group["markers"]}')
            continue
        identity = json.dumps([group['targets'], sorted(group['markers']), sorted(group['runner_tags'])])
        group_id = hashlib.sha256(identity.encode()).hexdigest()[:16]
        entry = tests_by_target[target].setdefault(group_id, {
            'id': group_id,
            'marker': ' and '.join(sorted(group['markers'])) or 'unmarked',
            'markers': sorted(group['markers']),
            **runner,
            'nodes': [],
        })
        entry['nodes'] = sorted(set(entry['nodes']) | set(group['nodes']))

    apps_matrix = []
    for target in sorted(tests_by_target):
        shards = shard_count(app_counts.get(target, 0), APPS_PER_SHARD, MAX_BUILD_SHARDS)
        if not shards:
            continue
        tests = [entry for _, entry in sorted(tests_by_target[target].items())]
        apps_matrix.append({
            'idf_target': target,
            'tests': tests,
            'run_tests': bool(tests),
            'parallel_indices': list(range(1, shards + 1)),
            'parallel_count': shards,
        })

    other_apps_count = sum(count for target, count in app_counts.items() if target not in tests_by_target)
    extra_shards = shard_count(other_apps_count, APPS_PER_SHARD, MAX_BUILD_SHARDS)
    extra_matrix = [
        {'parallel_index': index, 'parallel_count': extra_shards}
        for index in range(1, extra_shards + 1)
    ]
    return {'apps_matrix': {'include': apps_matrix}, 'extra_matrix': {'include': extra_matrix}}


def collect_discovery():
    build_collect = run_idf_ci('build', 'collect', '-p', '.', '--format', 'json')
    app_counts = Counter(
        app['target']
        for project in build_collect['projects'].values()
        for app in project['apps']
        if app['build_status'] == 'should be built'
    )
    eligible_apps = {
        # idf-build-apps names an implicit default configuration "", while
        # pytest-embedded calls it "default" and accepts build_<target>.
        (str(Path(path).resolve()), app['target'], app['config'] or 'default')
        for path, project in build_collect['projects'].items()
        for app in project['apps']
        if app['build_status'] == 'should be built'
        and not app.get('matched_rules', {}).get('disable_test')
        and not any(case.get('disabled_by_manifest', False) for case in app.get('test_cases', []))
    }
    return app_counts, collect_test_groups(eligible_apps)


def main() -> None:
    app_counts, test_groups = collect_discovery()
    print(f'Buildable apps per target: {dict(app_counts)}')
    print(f'Collected {len(test_groups)} test environment groups')
    matrices = generate_matrices(app_counts, test_groups)
    outputs = {key: json.dumps(value) for key, value in matrices.items()}
    outputs['has_apps'] = str(bool(matrices['apps_matrix']['include'])).lower()
    outputs['has_extra'] = str(bool(matrices['extra_matrix']['include'])).lower()
    outputs['idf_targets'] = json.dumps([entry['idf_target'] for entry in matrices['apps_matrix']['include']])
    with open(os.environ['GITHUB_OUTPUT'], 'a') as f:
        for key, value in outputs.items():
            f.write(f'{key}={value}\n')
    for key, value in outputs.items():
        print(f'{key}: {value}')


if __name__ == '__main__':
    main()
