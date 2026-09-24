#!/usr/bin/env python3
"""Generate dynamic build matrices for build_and_run_apps.yml.

GitHub Actions equivalent of the GitLab `generate_build_child_pipeline` job
(common/templates/idf/build.yml): instead of a fixed parallelism, count the
apps per target and size each target's shard count by the real app count
(ceil(apps / runs_per_job), capped at parallel_count from ci-matrix.json).

Outputs (written to $GITHUB_OUTPUT):
  apps_matrix   - {"include": [...]} entries for the per-target build+test
                  pipelines (one entry per idf_ver x idf_target)
  extra_matrix  - {"include": [...]} entries for the build-extra job
                  (all targets not listed in idf_targets)

Note: the app counting runs on one ESP-IDF version (the generator container),
so shard counts are approximate for other versions - this only affects load
balancing, not correctness.
"""

import json
import math
import os

from idf_build_apps import find_apps
from idf_build_apps.args import FindArguments

CI_MATRIX_PATH = '.github/ci-matrix.json'


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

    apps = find_apps(
        find_arguments=FindArguments(
            paths=['.'],
            recursive=True,
            enable_preview_targets=True,
            include_all_apps=True,
        )
    )
    app_counts = {}
    for app in apps:
        app_counts[app.target] = app_counts.get(app.target, 0) + 1
    print(f'App counts per target: {app_counts}')

    apps_matrix = []
    for target in tested_targets:
        shards = shard_count(app_counts.get(target, 0), runs_per_job, max_shards)
        if not shards:
            continue
        for idf_ver in idf_versions:
            apps_matrix.append(
                {
                    'idf_ver': idf_ver,
                    'idf_target': target,
                    'tests': test_configs[target],
                    'parallel_indices': list(range(1, shards + 1)),
                    'parallel_count': shards,
                }
            )

    other_apps_count = sum(count for target, count in app_counts.items() if target not in tested_targets)
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
