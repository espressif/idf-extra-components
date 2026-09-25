#!/usr/bin/env python3
"""Translate structured workflow inputs into idf-build-apps arguments."""

import argparse
import json
import os
import subprocess


def string_list(value):
    parsed = json.loads(value)
    if not isinstance(parsed, list) or any(not isinstance(item, str) for item in parsed):
        raise ValueError('Expected a JSON array of strings')
    if any(';' in item for item in parsed):
        raise ValueError('idf-build-apps cannot represent semicolons in list entries')
    return parsed


def build_command(target, parallel_index, parallel_count, info_file, modified_files, modified_components, disabled_targets):
    command = [
        'idf-build-apps', 'build', '--target', target,
        '--parallel-index', str(parallel_index), '--parallel-count', str(parallel_count),
        '--collect-app-info', info_file,
    ]
    for option, values in (
        ('--modified-files', modified_files), ('--modified-components', modified_components),
    ):
        if values:
            command.extend([option, ';'.join(values)])
    if disabled_targets:
        command.extend(['--disable-targets', *disabled_targets])
    return command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', default='all')
    parser.add_argument('--parallel-index', type=int, default=1)
    parser.add_argument('--parallel-count', type=int, default=1)
    parser.add_argument('--build-info', required=True)
    args = parser.parse_args()
    command = build_command(
        args.target, args.parallel_index, args.parallel_count, args.build_info,
        string_list(os.environ.get('MODIFIED_FILES', '[]')),
        string_list(os.environ.get('MODIFIED_COMPONENTS', '[]')),
        string_list(os.environ.get('DISABLED_TARGETS', '[]')),
    )
    raise SystemExit(subprocess.call(command))


if __name__ == '__main__':
    main()
