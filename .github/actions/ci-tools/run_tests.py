#!/usr/bin/env python3
"""Run exactly one discovered test group, respecting built-app selection."""

import argparse
import json
import os
import shlex


class SelectedNodes:
    """Fail if runtime collection differs from the post-build plan."""

    def __init__(self, nodes):
        if not isinstance(nodes, list) or not nodes or not all(isinstance(node, str) and node for node in nodes):
            raise ValueError('TEST_NODES must be a nonempty JSON array of pytest node IDs')
        self.nodes = set(nodes)

    def pytest_collection_finish(self, session):
        import pytest

        collected = {item.nodeid for item in session.items}
        if collected != self.nodes:
            raise pytest.UsageError(
                f'Runtime test collection differs from the build plan: '
                f'missing={sorted(self.nodes - collected)}, unexpected={sorted(collected - self.nodes)}'
            )


def main():
    import pytest

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', required=True)
    parser.add_argument('--junit-xml', required=True)
    args = parser.parse_args()
    selection = SelectedNodes(json.loads(os.environ['TEST_NODES']))
    # The planner already checked exact successful app/target/config tuples and
    # manifest permissions. Pass node IDs as arguments, never shell text or globs.
    command = [
        *sorted(selection.nodes),
        '--ignore-glob', '*/managed_components/*', '--ignore=.github',
        '--junit-xml', args.junit_xml, '--target', args.target,
        '--build-dir', f'build_{args.target}',
        *shlex.split(os.environ.get('PYTEST_ARGS', '')),
    ]
    raise SystemExit(pytest.main(command, plugins=[selection]))


if __name__ == '__main__':
    main()
