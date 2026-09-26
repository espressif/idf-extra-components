#!/usr/bin/env python3
"""Discover the target/shard matrix in the current ESP-IDF environment.

The caller owns the version matrix. This collector only handles one IDF and
never extrapolates application counts or test availability to other versions.
"""

import argparse
 import hashlib
import json
import math
import os
import subprocess
import sys
from collections import Counter


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


def collect_test_groups() -> list:
    # Use the same API as `idf-ci test collect`, retaining node IDs as an array.
    # Its GitHub formatter joins IDs with spaces, losing parameter IDs containing spaces.
    from idf_ci import get_pytest_cases

    cases = get_pytest_cases(marker_expr='', additional_args=['--suppress-no-test-exit-code'])
    # idf-ci adds an embedded_services parametrization specifically for --target
    # linux. Collect those IDs in the same context as the Linux worker.
    if any(case.targets == ['linux'] for case in cases):
        cases = [case for case in cases if case.targets != ['linux']] + get_pytest_cases(
            target='linux', marker_expr='', additional_args=['--suppress-no-test-exit-code'],
        )
    groups = {}
    for case in cases:
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


def resolve_runner(policy: dict, group: dict):
    """Translate discovered requirements into this caller's runner labels."""
    markers = set(group['markers'])
    # Emulator selection takes precedence: idf-ci also marks emulator cases host_test.
    environment = 'qemu' if 'qemu' in markers else 'host_test' if group['targets'] == ['linux'] else 'hardware'
    env = policy['environments'][environment]
    if 'targets' in env and not set(group['targets']).issubset(env['targets']):
        return None
    tags = set(group['runner_tags']) - {'self-hosted', 'host_test'}
    if environment != 'hardware':
        tags -= set(group['targets']) | {environment}
    for override in policy.get('overrides', []):
        if (override['targets'] == group['targets']
                and set(override['markers']).issubset(markers)):
            tags -= set(override['replace_tags'])
            tags.update(override['labels'])
    labels = list(env.get('labels', policy.get('default_labels', [])))
    for tag in sorted(tags):
        labels.extend(policy.get('tag_aliases', {}).get(tag, [tag]))
    return {
        'runner_labels': list(dict.fromkeys(labels)),
        'pytest_args': env.get('pytest_args', ''),
        'setup_qemu': environment == 'qemu',
    }


def generate_matrices(cfg: dict, app_counts: Counter, test_groups: list, profile: str = 'default') -> dict:
    """Plan complete discovered groups using the project's infrastructure policy."""
    max_shards = int(cfg['parallel_count'])
    runs_per_job = int(cfg['runs_per_job'])
    if max_shards <= 0 or runs_per_job <= 0:
        raise ValueError('parallel_count and runs_per_job must be positive')
    profiles = cfg.get('profiles', {'default': {}})
    if profile not in profiles:
        raise ValueError(f'Unknown test profile: {profile}')
    excluded_markers = set(profiles[profile].get('exclude_markers', []))
    tested_targets = cfg['idf_targets']
    if len(tested_targets) != len(set(tested_targets)):
        raise ValueError('idf_targets must be unique')
    if 'linux' not in tested_targets and app_counts.get('linux', 0):
        raise ValueError('Add linux to idf_targets to build host applications separately')

    tests_by_target = {target: {} for target in tested_targets}
    for group in test_groups:
        if not group['nodes'] or excluded_markers.intersection(group['markers']):
            continue
        # The reusable worker builds and restores one target. Never silently split
        # a multi-DUT group into jobs that cannot provide its required devices.
        if len(group['targets']) != 1:
            raise ValueError(f'Multi-DUT test groups are not supported by the single-target worker: {group["targets"]}')
        target = group['targets'][0]
        if target not in tests_by_target or not app_counts.get(target, 0):
            continue
        runner = resolve_runner(cfg['runner_policy'], group)
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
    for target in tested_targets:
        shards = shard_count(app_counts.get(target, 0), runs_per_job, max_shards)
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

    other_apps_count = sum(count for target, count in app_counts.items() if target not in tested_targets)
    extra_shards = shard_count(other_apps_count, runs_per_job, max_shards)
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
    return app_counts, collect_test_groups()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', default='.github/ci-config.json')
    parser.add_argument('--profile', default='default')
    args = parser.parse_args()
    with open(args.config) as f:
        cfg = json.load(f)
    app_counts, test_groups = collect_discovery()
    print(f'Buildable apps per target: {dict(app_counts)}')
    print(f'Collected {len(test_groups)} test environment groups')
    matrices = generate_matrices(cfg, app_counts, test_groups, args.profile)
    outputs = {key: json.dumps(value) for key, value in matrices.items()}
    outputs['has_apps'] = str(bool(matrices['apps_matrix']['include'])).lower()
    outputs['has_extra'] = str(bool(matrices['extra_matrix']['include'])).lower()
    outputs['idf_targets'] = json.dumps(cfg['idf_targets'])
    with open(os.environ['GITHUB_OUTPUT'], 'a') as f:
        for key, value in outputs.items():
            f.write(f'{key}={value}\n')
    for key, value in outputs.items():
        print(f'{key}: {value}')


if __name__ == '__main__':
    main()
