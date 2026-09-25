#!/usr/bin/env python3
"""Discover the target/shard matrix in the current ESP-IDF environment.

The caller owns the version matrix. This collector only handles one IDF and
never extrapolates application counts or test availability to other versions.
"""

import argparse
import json
import math
import os
import subprocess
import sys
from collections import Counter, defaultdict


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


def generate_matrices(cfg: dict, app_counts: Counter, markers_by_target: dict, profile: str = 'default') -> dict:
    """Intersect this IDF's discovery with the project's runner policy."""
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

    apps_matrix = []
    for target in tested_targets:
        configs = cfg.get('test_configs', {}).get(target, [])
        markers = [config['marker'] for config in configs]
        if len(markers) != len(set(markers)):
            raise ValueError(f'Test markers must be unique for target {target}')
        shards = shard_count(app_counts.get(target, 0), runs_per_job, max_shards)
        if not shards:
            continue
        tests = [
            config for config in configs
            if config['marker'] in markers_by_target.get(target, set())
            and config['marker'] not in excluded_markers
        ]
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
    # idf-ci treats QEMU as a host/emulator case. Collect all three groups.
    markers_by_target = defaultdict(set)
    for marker_expr in ('not host_test', 'qemu', 'host_test'):
        collection = run_idf_ci('test', 'collect', '--format', 'github', '-m', marker_expr)
        for entry in collection['include']:
            if not entry['nodes'].strip():
                continue
            markers = {m for m in entry['env_markers'].split(' and ') if m}
            for target in entry['targets'].split(','):
                markers_by_target[target].update(markers)
                if target == 'linux' and marker_expr == 'host_test':
                    markers_by_target[target].add('host_test')
    return app_counts, markers_by_target


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', default='.github/ci-config.json')
    parser.add_argument('--profile', default='default')
    args = parser.parse_args()
    with open(args.config) as f:
        cfg = json.load(f)
    app_counts, markers_by_target = collect_discovery()
    print(f'Buildable apps per target: {dict(app_counts)}')
    print(f'Collected test markers per target: {dict(markers_by_target)}')
    matrices = generate_matrices(cfg, app_counts, markers_by_target, args.profile)
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
