#!/usr/bin/env python3
"""Discover build shards for one ESP-IDF version, without collecting tests."""

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
    from idf_build_apps.args import FindArguments
    from idf_build_apps.constants import BuildStatus

    # Preview targets include Linux. Enabling previews globally would implicitly
    # allow every unrestricted hardware app on the host (including sdmmc users).
    # Keep hardware previews, then discover Linux with the normal manifest defaults.
    hardware_apps = find_apps(find_arguments=FindArguments(
        paths=['.'], recursive=True, enable_preview_targets=True,
    ))
    apps = [app for app in hardware_apps if app.target != 'linux']
    apps.extend(find_apps(find_arguments=FindArguments(
        paths=['.'], target='linux', recursive=True, enable_preview_targets=False,
    )))
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
