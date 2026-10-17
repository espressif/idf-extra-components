"""JUnit aggregation preserves outcomes, context and incomplete-result evidence."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'actions/ci-tools/generate_test_report.py'
SPEC = importlib.util.spec_from_file_location('generate_test_report', SCRIPT)
REPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPORT)


class TestReportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def write(self, directory, xml=None, **context):
        path = self.root / directory
        path.mkdir()
        if xml is not None:
            (path / 'test_results.xml').write_text(xml)
        if context:
            (path / 'test-context.json').write_text(json.dumps(context))
        return path

    def test_outcomes_come_from_xml_elements_not_error_words_in_logs(self):
        self.write('job', '''<testsuites><testsuite name="pytest">
        <testcase name="negative test" app_path="/__w/repo/repo/component/test_apps" is_unity_case="1" time="1.5">
          <system-out>ERROR: expected invalid argument, test PASS</system-out></testcase>
        <testcase name="assertion"><failure message="Expected 1">Got 2</failure></testcase>
        <testcase name="fixture"><error message="No DUT"/></testcase>
        <testcase name="unsupported"><skipped message="No PSRAM"/></testcase>
        </testsuite></testsuites>''', idf_version='release-v5.3', target='esp32', marker='generic')
        report = REPORT.collect_report(self.root)
        self.assertEqual([r['status'] for r in report['records']], ['passed', 'failed', 'error', 'skipped'])
        self.assertEqual(report['records'][0]['app'], 'component/test_apps')
        self.assertEqual(report['records'][0]['kind'], 'Unity')
        self.assertEqual(report['records'][0]['duration'], 1.5)
        self.assertEqual(report['records'][1]['message'], 'Expected 1\nGot 2')
        self.assertEqual(report['issues'], [])

    def test_same_named_cases_in_different_sdk_jobs_are_not_deduplicated(self):
        for version in ['v5.3', 'v6.1']:
            self.write(version, '<testsuite><testcase name="same"/></testsuite>',
                       idf_version=version, target='esp32', marker='generic')
        report = REPORT.collect_report(self.root)
        self.assertEqual(len(report['records']), 2)
        self.assertEqual({r['idf_version'] for r in report['records']}, {'v5.3', 'v6.1'})
        self.assertEqual(len({r['source'] for r in report['records']}), 2)

    def test_nested_namespaced_suites_do_not_double_count_cases(self):
        self.write('job', '<testsuites xmlns="urn:junit"><testsuite name="outer"><testsuite name="inner">'
                         '<testcase name="nested" time="nan"/></testsuite></testsuite></testsuites>')
        report = REPORT.collect_report(self.root)
        self.assertEqual(len(report['records']), 1)
        self.assertEqual(report['records'][0]['suite'], 'inner')
        self.assertEqual(report['records'][0]['duration'], 0)
        self.assertEqual(report['records'][0]['target'], 'Unknown')
        self.assertTrue(report['issues'])

    def test_missing_malformed_and_empty_xml_are_visible_not_passed(self):
        self.write('missing', job_result='failure', run_result='skipped', target='esp32')
        self.write('malformed', '<testsuite><testcase')
        self.write('empty', '<testsuite tests="0"/>')
        self.write('valid', '<testsuite><testcase name="valid"/></testsuite>')
        report = REPORT.collect_report(self.root)
        self.assertEqual(len(report['records']), 1)
        self.assertEqual(len(report['groups']), 4)
        self.assertTrue(any('no JUnit XML' in issue for issue in report['issues']))
        self.assertTrue(any('cannot read results' in issue for issue in report['issues']))
        self.assertTrue(any('no test cases' in issue for issue in report['issues']))
        self.assertEqual(REPORT.collect_report(self.root / 'absent')['records'], [])
        self.assertTrue(REPORT.collect_report(self.root / 'absent')['issues'])

    def test_invalid_context_preserves_xml_with_unknown_identity(self):
        path = self.write('job', '<testsuite><testcase name="valid"/></testsuite>')
        (path / 'test-context.json').write_text('[]')
        report = REPORT.collect_report(self.root)
        self.assertEqual(len(report['records']), 1)
        self.assertEqual(report['records'][0]['idf_version'], 'Unknown')
        self.assertTrue(any('test-context.json' in issue for issue in report['issues']))

    def test_untrusted_xml_content_is_inert_in_html(self):
        self.write('job', '<testsuite><testcase name="&lt;/script&gt;&lt;script&gt;alert(1)&lt;/script&gt;">'
                         '<failure>&lt;img src=x onerror=alert(1)&gt;</failure></testcase></testsuite>')
        report = REPORT.collect_report(self.root)
        output = self.root / 'report.html'
        REPORT.render_report(report, output)
        html = output.read_text()
        self.assertNotIn('</script><script>alert(1)', html)
        self.assertNotIn('<img src=x', html)
        data = html.split('<script type="application/json" id="test-data">', 1)[1].split('</script>', 1)[0]
        self.assertEqual(json.loads(data), report)


if __name__ == '__main__':
    unittest.main()
