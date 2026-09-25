#!/usr/bin/env python3
"""Archive runtime files at their workspace-relative paths using build metadata.

A tar archive preserves both the directory layout (including root-level apps)
and executable permissions of Linux binaries across upload/download-artifact.
"""

import argparse
import json
from pathlib import Path
import tarfile


def runtime_files(build_path):
    for path in build_path.rglob('*'):
        if path.is_file() and (
            path.suffix in ('.bin', '.elf')
            or path.name == 'flasher_args.json'
            or path.relative_to(build_path).as_posix() == 'config/sdkconfig.json'
        ):
            yield path


def package(info_file, output, root):
    root = root.resolve()
    paths = {info_file.resolve()}
    for line in info_file.read_text().splitlines():
        app = json.loads(line)
        if app['build_status'] != 'success':
            continue
        # idf-build-apps serializes expanded work_dir and build_dir fields.
        build_path = Path(app['build_dir'])
        if not build_path.is_absolute():
            build_path = root / app['work_dir'] / build_path
        build_path = build_path.resolve()
        build_path.relative_to(root)
        paths.update(path.resolve() for path in runtime_files(build_path))
    with tarfile.open(output, 'w') as archive:
        for path in sorted(paths):
            archive.add(path, arcname=path.relative_to(root).as_posix(), recursive=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build_info', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    package(args.build_info, args.output, Path.cwd())


if __name__ == '__main__':
    main()
