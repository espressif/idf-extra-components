#!/usr/bin/env python3
"""Aggregate pytest/Unity JUnit XML into a self-contained, offline HTML report."""

import argparse
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import re
import xml.etree.ElementTree as ET


def seconds(value):
    try:
        number = float(value or 0)
        return number if math.isfinite(number) and number >= 0 else 0
    except (ValueError, TypeError):
        return 0


def read_cases(path):
    data = path.read_bytes()
    if b'<!DOCTYPE' in data.upper():
        raise ValueError('DOCTYPE declarations are not supported')
    root = ET.fromstring(data)
    for node in root.iter():
        node.tag = node.tag.rsplit('}', 1)[-1]
    if root.tag not in {'testsuites', 'testsuite'}:
        raise ValueError('Expected a JUnit testsuite or testsuites root')
    records = []
    for suite in root.iter('testsuite'):
        for case in suite.findall('testcase'):
            status = next((tag for tag in ('error', 'failure', 'skipped') if case.find(tag) is not None), 'passed')
            status = {'failure': 'failed'}.get(status, status)
            messages = []
            for tag in ('error', 'failure', 'skipped'):
                for node in case.findall(tag):
                    messages.append('\n'.join(filter(None, [node.get('message'), ''.join(node.itertext()).strip()])))
            app = case.get('app_path') or case.get('classname') or case.get('file') or 'Unknown application'
            app = re.sub(r'^/__w/([^/]+)/\1/', '', app)
            records.append({
                'app': app, 'name': case.get('name', '(unnamed test)'),
                'classname': case.get('classname', ''), 'file': case.get('file', ''),
                'line': case.get('line', ''), 'suite': suite.get('name', ''),
                'kind': 'Unity' if case.get('is_unity_case') == '1' else 'JUnit',
                'status': status, 'duration': seconds(case.get('time')),
                'message': '\n\n'.join(messages),
                'stdout': '\n'.join(''.join(n.itertext()) for n in case.findall('system-out')),
                'stderr': '\n'.join(''.join(n.itertext()) for n in case.findall('system-err')),
            })
    return records


def collect_report(directory, defaults=None, pipeline_result='unknown'):
    defaults = defaults or {}
    records, groups, issues = [], [], []
    contexts = {}
    for path in sorted(directory.rglob('test-context.json')):
        try:
            context = json.loads(path.read_text())
            if not isinstance(context, dict) or any(not isinstance(v, str) for v in context.values()):
                raise ValueError('Expected an object with string values')
            contexts[path.parent] = context
        except (ValueError, OSError) as error:
            issues.append(f'{path.relative_to(directory)}: {error}')
    xml_files = sorted(directory.rglob('*.xml'))
    for path in xml_files:
        source = str(path.relative_to(directory))
        context = {**defaults, **contexts.get(path.parent, {})}
        identity = {key: context.get(key) or 'Unknown' for key in ('idf_version', 'target', 'marker')}
        group = {**identity, 'source': source, 'job_result': context.get('job_result', 'unknown'),
                 'run_result': context.get('run_result', 'unknown'), 'count': 0}
        groups.append(group)
        try:
            cases = read_cases(path)
        except (ET.ParseError, ValueError, OSError) as error:
            issues.append(f'{source}: cannot read results: {error}')
            continue
        group['count'] = len(cases)
        records.extend({**case, **identity, 'source': source} for case in cases)
        if not cases:
            issues.append(f'{source}: no test cases recorded')
    for parent, context in contexts.items():
        if not any(path.parent == parent for path in xml_files):
            source = str((parent / 'test-context.json').relative_to(directory))
            groups.append({**{key: context.get(key, 'Unknown') for key in ('idf_version', 'target', 'marker')},
                           'source': source, 'count': 0, 'job_result': context.get('job_result', 'unknown'),
                           'run_result': context.get('run_result', 'unknown')})
            issues.append(f'{source}: job supplied no JUnit XML; test outcomes are unavailable')
    if not xml_files:
        issues.append('No JUnit XML files were downloaded. This does not mean that tests passed or were skipped.')
    if any('Unknown' in (row['idf_version'], row['target'], row['marker']) for row in records):
        issues.append('Some XML files have no SDK/target/environment context; these fields are shown as Unknown.')
    return {
        'generated_at': datetime.now(timezone.utc).isoformat(timespec='seconds'),
        'repository': os.environ.get('GITHUB_REPOSITORY', ''),
        'commit': os.environ.get('GITHUB_SHA', ''), 'run_id': os.environ.get('GITHUB_RUN_ID', ''),
        'pipeline_result': pipeline_result, 'xml_files': len(xml_files),
        'records': records, 'groups': groups, 'issues': issues,
    }


def render_report(report, output):
    data = json.dumps(report, ensure_ascii=True).replace('&', '\\u0026').replace('<', '\\u003c').replace('>', '\\u003e')
    template = Path(__file__).with_name('test_report.html').read_text()
    template = template.replace('/* REPORT_CSS */', Path(__file__).with_name('report.css').read_text())
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(template.replace('<!-- TEST_REPORT_DATA -->', data))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results_dir', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--idf-version', default='Unknown')
    parser.add_argument('--target', default='Unknown')
    parser.add_argument('--marker', default='Unknown')
    parser.add_argument('--pipeline-result', default='unknown')
    args = parser.parse_args()
    report = collect_report(args.results_dir, dict(idf_version=args.idf_version, target=args.target, marker=args.marker), args.pipeline_result)
    render_report(report, args.output)
    print(f'Wrote {args.output}: {len(report["records"])} test cases from {report["xml_files"]} XML files')
    # Report generation does not replace the actual test jobs' exit status.
    # Missing and malformed inputs remain visible in the diagnostic HTML.
    if report['issues']:
        print('\n'.join(report['issues']))


if __name__ == '__main__':
    main()
