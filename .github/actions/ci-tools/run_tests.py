#!/usr/bin/env python3
"""Run exactly one discovered test group, respecting built-app selection."""

import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys


class SelectedNodes:
    """Filter after collection, so build metadata's --ignore rules still apply.

    Passing node IDs as positional pytest arguments would bypass directory ignores.
    Keeping IDs as JSON also preserves parametrizations containing whitespace.
    """

    def __init__(self, nodes):
        if not isinstance(nodes, list) or not all(isinstance(node, str) for node in nodes):
            raise ValueError('TEST_NODES must be a JSON array of pytest node IDs')
        self.nodes = set(nodes)

    def pytest_collection_modifyitems(self, config, items):
        selected, deselected = [], []
        for item in items:
            (selected if item.nodeid in self.nodes else deselected).append(item)
        items[:] = selected
        if deselected:
            config.hook.pytest_deselected(items=deselected)


def main():
    import pytest

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', required=True)
    parser.add_argument('--junit-xml', required=True)
    args = parser.parse_args()
    selection = SelectedNodes(json.loads(os.environ['TEST_NODES']))
    selector = Path(__file__).with_name('get_pytest_args.py')
    subprocess.run([
        sys.executable, str(selector), '--target', args.target,
        '-v', 'build_info*.json', 'pytest-args.txt',
    ], check=True)
    command = [
        '--suppress-no-test-exit-code',
        *shlex.split(Path('pytest-args.txt').read_text()),
        '--ignore-glob', '*/managed_components/*', '--ignore=.github',
        '--junit-xml', args.junit_xml, '--target', args.target,
        '--build-dir', f'build_{args.target}',
        *shlex.split(os.environ.get('PYTEST_ARGS', '')),
    ]
    raise SystemExit(pytest.main(command, plugins=[selection]))


if __name__ == '__main__':
    main()
