#!/usr/bin/env python3
"""Select changed root-level components for this repository's CI caller."""

import argparse
import json
import os
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('changed_files', type=Path)
    args = parser.parse_args()
    files = args.changed_files.read_text().splitlines()
    components = sorted({
        file.split('/')[0] for file in files
        if file.split('/')[0] != '.github' and Path(file.split('/')[0]).is_dir()
    })
    # Preserve the existing component-level selection policy. The reusable
    # workflow receives data, never a shell fragment specific to this layout.
    outputs = {
        'modified_files': json.dumps(files),
        'modified_components': json.dumps(components),
        'has_changes': str(bool(components)).lower(),
    }
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        for key, value in outputs.items():
            output.write(f'{key}={value}\n')


if __name__ == '__main__':
    main()
