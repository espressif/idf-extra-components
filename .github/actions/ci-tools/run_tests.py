#!/usr/bin/env python3
"""Run pytest with built-app selection and the configured runner profile."""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', required=True)
    parser.add_argument('--marker', required=True)
    parser.add_argument('--junit-xml', required=True)
    args = parser.parse_args()
    selector = Path(__file__).with_name('get_pytest_args.py')
    subprocess.run([
        sys.executable, str(selector), '--target', args.target,
        '-v', 'build_info*.json', 'pytest-args.txt',
    ], check=True)
    command = [
        'pytest', '--suppress-no-test-exit-code',
        *shlex.split(Path('pytest-args.txt').read_text()),
        '--ignore-glob', '*/managed_components/*', '--ignore=.github',
        '--junit-xml', args.junit_xml, '--target', args.target,
        '-m', args.marker, '--build-dir', f'build_{args.target}',
        *shlex.split(os.environ.get('PYTEST_ARGS', '')),
    ]
    raise SystemExit(subprocess.call(command))


if __name__ == '__main__':
    main()
