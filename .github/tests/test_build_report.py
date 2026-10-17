"""Build reports must distinguish actual results, missing metadata and errors."""

import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / 'actions/ci-tools/generate_build_report.py'
SPEC = importlib.util.spec_from_file_location('generate_build_report', SCRIPT)
REPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPORT)


class BuildReportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.matrix = {'include': [
            {'idf_target': 'esp32', 'parallel_index': 1, 'parallel_count': 2},
            {'idf_target': 'esp32', 'parallel_index': 2, 'parallel_count': 2},
        ]}

    def write(self, index, records):
        path = self.root / f'build_info_esp32_{index}.json'
        path.write_text(''.join(json.dumps(record) + '\n' for record in records))
        return path

    def record(self, status='build success', **kwargs):
        return dict(app_dir='app/test_apps', target='esp32', config_name='', build_status=status, **kwargs)

    def test_report_preserves_actual_variants_statuses_and_reasons(self):
        success = self.record()
        failure = self.record('build failed', build_comment='Compiler error')
        failure['config_name'] = 'psram'
        self.write(1, [success, failure])
        self.write(2, [self.record('skipped', build_comment='No affected dependencies')])
        report = REPORT.collect_report(self.root, self.matrix, 'release-v6.1', 'failure')
        self.assertEqual(report['build_result'], 'failure')
        self.assertEqual(report['received_shards'], 2)
        self.assertEqual(report['missing'], [])
        self.assertEqual(len(report['records']), 3)
        self.assertEqual({r['status'] for r in report['records']}, {'build success', 'build failed', 'skipped'})
        self.assertEqual(next(r['reason'] for r in report['records'] if r['status'] == 'build failed'), 'Compiler error')
        self.assertEqual(next(r['config'] for r in report['records'] if r['status'] == 'build success'), 'default')

    def test_missing_shards_are_not_invented_as_skipped_apps(self):
        self.write(1, [self.record()])
        report = REPORT.collect_report(self.root, self.matrix, 'latest', 'failure')
        self.assertEqual(report['missing'], ['build_info_esp32_2.json'])
        self.assertEqual(len(report['records']), 1)
        self.assertEqual(report['received_shards'], 1)

    def test_empty_shards_and_no_builds_are_valid_empty_reports(self):
        self.write(1, [])
        self.write(2, [])
        report = REPORT.collect_report(self.root, self.matrix, 'latest', 'success')
        self.assertEqual(report['records'], [])
        self.assertEqual(report['missing'], [])
        report = REPORT.collect_report(self.root / 'absent', {'include': []}, 'latest', 'skipped')
        self.assertEqual(report['expected_shards'], 0)
        self.assertEqual(report['errors'], [])

    def test_partial_or_invalid_json_keeps_valid_results_and_reports_error(self):
        path = self.write(1, [self.record()])
        with path.open('a') as output:
            output.write('{"interrupted":\n[]\n')
        self.write(2, [])
        report = REPORT.collect_report(self.root, self.matrix, 'latest', 'failure')
        self.assertEqual(len(report['records']), 1)
        self.assertEqual(len(report['errors']), 2)
        self.assertIn('build_info_esp32_1.json:2:', report['errors'][0])

    def test_unexpected_metadata_is_visible(self):
        self.write(3, [self.record()])
        report = REPORT.collect_report(self.root, self.matrix, 'latest', 'failure')
        self.assertEqual(report['unexpected'], ['build_info_esp32_3.json'])
        self.assertEqual(report['records'][0]['shard'], 'unknown')

    def test_html_keeps_untrusted_build_text_inert_and_round_trips(self):
        attack = '</script><script>alert("unexpected")</script>&<img src=x onerror=alert(1)>'
        self.write(1, [self.record('build failed', build_comment=attack)])
        self.write(2, [])
        report = REPORT.collect_report(self.root, self.matrix, attack, 'failure')
        output = self.root / 'report.html'
        REPORT.render_report(report, output)
        html = output.read_text()
        self.assertNotIn(attack, html)
        data = html.split('<script type="application/json" id="build-data">', 1)[1].split('</script>', 1)[0]
        self.assertEqual(json.loads(data), report)
        self.assertNotIn('<script src=', html)
        self.assertNotIn('<!-- BUILD_REPORT_DATA -->', html)

    def test_incomplete_cli_writes_downloadable_report_before_failing(self):
        output = self.root / 'report.html'
        with patch.dict(os.environ, {'BUILD_MATRIX': json.dumps(self.matrix)}), \
                patch('sys.argv', ['report', str(self.root), str(output), '--idf-version', 'latest', '--build-result', 'failure']):
            with self.assertRaises(SystemExit) as error:
                REPORT.main()
        self.assertEqual(error.exception.code, 1)
        self.assertTrue(output.is_file())


if __name__ == '__main__':
    unittest.main()
