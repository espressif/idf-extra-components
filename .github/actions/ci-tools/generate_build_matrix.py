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
    from idf_build_apps import find_apps
    from idf_build_apps.constants import SUPPORTED_TARGETS, BuildStatus
    from idf_ci import get_all_apps

    # Include build-only apps and all test environments. Runtime jobs are still
    # selected separately, after successful builds.
    test_apps, other_apps = get_all_apps(marker_expr='')
    apps = [*test_apps, *other_apps]
    # Host apps opt into linux builds via manifest `enable:
    # IDF_TARGET == "linux"` rules; the target='all' discovery above iterates
    # SDK targets only and never evaluates them. Discover linux separately
    # with the same semantics as the previous dedicated `--target linux` job:
    # linux is a default target here, and manifest enable rules select the
    # actual host apps. The pytest side links linux cases independently of
    # this, so plan-tests eligibility follows the build_info files.
    apps += find_apps(['.'], 'linux', default_build_targets=list(SUPPORTED_TARGETS))
    return Counter(app.target for app in apps if app.build_status == BuildStatus.SHOULD_BE_BUILT)


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
