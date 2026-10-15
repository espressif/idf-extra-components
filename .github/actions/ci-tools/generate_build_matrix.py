#!/usr/bin/env python3
"""Discover build shards for one ESP-IDF version through idf-ci."""

from collections import Counter
import json
import math
import os

APPS_PER_SHARD = 20


def generate_build_matrix(app_counts):
    jobs = []
    for target, count in sorted(app_counts.items()):
        shards = math.ceil(count / APPS_PER_SHARD)
        jobs.extend(
            {'idf_target': target, 'parallel_index': index, 'parallel_count': shards}
            for index in range(1, shards + 1)
        )
    return {'include': jobs}


def collect_app_counts():
    from idf_build_apps.constants import BuildStatus
    from idf_ci import get_all_apps

    # Include build-only apps and all test environments. Runtime jobs are still
    # selected separately, after successful builds.
    test_apps, other_apps = get_all_apps(marker_expr='')
    return Counter(app.target for app in [*test_apps, *other_apps]
                   if app.build_status == BuildStatus.SHOULD_BE_BUILT)


def main():
    counts = collect_app_counts()
    print(f'Buildable apps per target: {dict(counts)}')
    matrix = generate_build_matrix(counts)
    print(json.dumps(matrix, indent=2))
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        output.write(f'build_matrix={json.dumps(matrix)}\n')
        output.write(f'has_builds={str(bool(matrix["include"])).lower()}\n')


if __name__ == '__main__':
    main()
