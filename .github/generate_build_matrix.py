#!/usr/bin/env python3
"""Generate dynamic build/test matrices for build_and_run_apps.yml via idf-ci.

GitHub Actions equivalent of the GitLab `generate_build_child_pipeline` job
(common/templates/idf/build.yml): instead of a fixed parallelism, ask idf-ci
what actually exists and size everything from that:

- `idf-ci build collect`  -> buildable apps per target (build_status ==
  "should be built") -> shard count = ceil(apps / runs_per_job), capped at
  parallel_count
- `idf-ci test collect --format github` -> which env markers (generic,
  ethernet, qemu, ...) have real test cases per target; the per-target test
  configs in ci-matrix.json act as the runner-availability policy and are
  intersected with the collected markers

Outputs (written to $GITHUB_OUTPUT):
  apps_matrix   - {"include": [...]} entries for the per-target build+test
                  pipelines (one entry per idf_ver x idf_target)
  extra_matrix  - {"include": [...]} entries for the build-extra job
                  (all non-linux targets not listed in idf_targets)

Note: collection runs on one ESP-IDF version (the generator container), so
counts are approximate for other versions - this only affects load balancing,
not correctness.
"""

import json
import math
import os
import subprocess
import sys
from collections import Counter, defaultdict

CI_MATRIX_PATH = '.github/ci-matrix.json'


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
    return max(1, min(math.ceil(app_count / runs_per_job), max_shards))


def main() -> None:
    with open(CI_MATRIX_PATH) as f:
        cfg = json.load(f)

    idf_versions = cfg['idf_versions']
    tested_targets = cfg['idf_targets']
    test_configs = cfg['test_configs']
    max_shards = int(cfg['parallel_count'])
    runs_per_job = int(cfg['runs_per_job'])

    # Buildable apps per target
    build_collect = run_idf_ci('build', 'collect', '-p', '.', '--format', 'json')
    app_counts: Counter = Counter()
    for project in build_collect['projects'].values():
        for app in project['apps']:
            if app['build_status'] == 'should be built':
                app_counts[app['target']] += 1
    print(f'Buildable apps per target: {dict(app_counts)}')

    # Env markers with real test cases per target. qemu cases are treated as
    # host tests by idf-ci (emulator marker), so they need a separate collect.
    markers_by_target: dict = defaultdict(set)
    for marker_expr in ('not host_test', 'qemu'):
        test_collect = run_idf_ci('test', 'collect', '--format', 'github', '-m', marker_expr)
        for entry in test_collect['include']:
            if not entry['nodes'].strip():
                continue
            markers = [m for m in entry['env_markers'].split(' and ') if m]
            for target in entry['targets'].split(','):
                markers_by_target[target].update(markers)
    print(f'Collected test markers per target: {dict(markers_by_target)}')

    apps_matrix = []
    for target in tested_targets:
        shards = shard_count(app_counts.get(target, 0), runs_per_job, max_shards)
        if not shards:
            continue
        # runner policy from ci-matrix.json, filtered to markers with cases
        tests = [c for c in test_configs.get(target, []) if c['marker'] in markers_by_target.get(target, set())]
        for idf_ver in idf_versions:
            apps_matrix.append(
                {
                    'idf_ver': idf_ver,
                    'idf_target': target,
                    'tests': tests,
                    'run_tests': bool(tests),
                    'parallel_indices': list(range(1, shards + 1)),
                    'parallel_count': shards,
                }
            )

    other_apps_count = sum(
        count for target, count in app_counts.items() if target not in tested_targets and target != 'linux'
    )
    extra_shards = shard_count(other_apps_count, runs_per_job, max_shards)
    extra_matrix = [
        {'idf_ver': idf_ver, 'parallel_index': index, 'parallel_count': extra_shards}
        for idf_ver in idf_versions
        for index in range(1, extra_shards + 1)
    ]

    outputs = {
        'apps_matrix': json.dumps({'include': apps_matrix}),
        'extra_matrix': json.dumps({'include': extra_matrix}),
    }
    with open(os.environ['GITHUB_OUTPUT'], 'a') as f:
        for key, value in outputs.items():
            f.write(f'{key}={value}\n')
    for key, value in outputs.items():
        print(f'{key}: {value}')


if __name__ == '__main__':
    main()
