#!/usr/bin/env python3
"""Render an offline HTML report from recorded idf-build-apps outcomes."""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path


def collect_report(info_dir, build_matrix, idf_version, build_result):
    expected = {
        f'build_info_{job["idf_target"]}_{job["parallel_index"]}.json': job
        for job in build_matrix['include']
    }
    files = {path.name: path for path in info_dir.glob('build_info_*.json')}
    records, errors = [], []
    for name, path in sorted(files.items()):
        for line_number, line in enumerate(path.read_text().splitlines(), 1):
            if not line.strip():
                continue
            try:
                app = json.loads(line)
                if not isinstance(app, dict) or any(
                    not isinstance(app.get(key), str) for key in ('app_dir', 'target', 'build_status')
                ):
                    raise ValueError('Expected app_dir, target and build_status strings')
                for key in ('config_name', 'build_comment', 'build_dir', 'work_dir', 'sdkconfig_path'):
                    if app.get(key) is not None and not isinstance(app[key], str):
                        raise ValueError(f'Expected {key} to be a string or null')
            except (ValueError, TypeError) as error:
                errors.append(f'{name}:{line_number}: {error}')
                continue
            job = expected.get(name, {})
            records.append({
                'app': app['app_dir'],
                'target': app['target'],
                'config': app.get('config_name') or 'default',
                'status': app['build_status'],
                'reason': app.get('build_comment') or '',
                'build_dir': app.get('build_dir') or '',
                'work_dir': app.get('work_dir') or app['app_dir'],
                'sdkconfig': app.get('sdkconfig_path') or '',
                'source': name,
                'shard': f'{job["parallel_index"]}/{job["parallel_count"]}' if job else 'unknown',
            })
    return {
        'idf_version': idf_version,
        'build_result': build_result,
        'generated_at': datetime.now(timezone.utc).isoformat(timespec='seconds'),
        'repository': os.environ.get('GITHUB_REPOSITORY', ''),
        'commit': os.environ.get('GITHUB_SHA', ''),
        'run_id': os.environ.get('GITHUB_RUN_ID', ''),
        'attempt': os.environ.get('GITHUB_RUN_ATTEMPT', ''),
        'expected_shards': len(expected),
        'received_shards': len(expected.keys() & files.keys()),
        'missing': sorted(expected.keys() - files.keys()),
        'unexpected': sorted(files.keys() - expected.keys()),
        'errors': errors,
        'records': sorted(records, key=lambda row: (row['app'], row['target'], row['config'], row['source'])),
    }


def render_report(report, output):
    # JSON is data, never markup: closing script tags and HTML in build errors
    # must remain inert when a report from an untrusted PR is opened locally.
    data = json.dumps(report, ensure_ascii=True).replace('&', '\\u0026').replace('<', '\\u003c').replace('>', '\\u003e')
    template = Path(__file__).with_name('build_report.html').read_text()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(template.replace('<!-- BUILD_REPORT_DATA -->', data))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build_info_dir', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--idf-version', required=True)
    parser.add_argument('--build-result', choices=['success', 'failure', 'cancelled', 'skipped'], required=True)
    args = parser.parse_args()
    report = collect_report(args.build_info_dir, json.loads(os.environ['BUILD_MATRIX']), args.idf_version, args.build_result)
    render_report(report, args.output)
    print(f'Wrote {args.output}: {len(report["records"])} recorded variants; '
          f'{report["received_shards"]}/{report["expected_shards"]} shard reports')
    # Preserve and upload the diagnostic HTML, but do not silently pass an
    # incomplete report. A failed compilation is separately kept red by build.
    if report['missing'] or report['unexpected'] or report['errors']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
