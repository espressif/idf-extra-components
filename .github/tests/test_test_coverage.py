"""Coverage checks distinguish missing builds from intentional test exclusions."""

import importlib.util
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from contextlib import redirect_stdout

SCRIPT = Path(__file__).resolve().parents[1] / 'actions/ci-tools/check_test_coverage.py'
SPEC = importlib.util.spec_from_file_location('check_test_coverage', SCRIPT)
COVERAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COVERAGE)


def app(config, cases=(), target='esp32'):
    return dict(target=target, config=config, test_cases=list(cases))


class CoverageTests(unittest.TestCase):
    def test_error_annotation_identifies_variant_and_tests_and_escapes_newlines(self):
        output = io.StringIO()
        with redirect_stdout(output):
            COVERAGE.report_errors([dict(app='app', target='linux', config='default',
                                         tests=['test.py::test[linux]', 'test.py::test[100%\nvalue]'])])
        message = output.getvalue()
        self.assertIn('app (target=linux, config=default)', message)
        self.assertIn('test.py::test[linux]', message)
        self.assertIn('test.py::test[100%25%0Avalue]', message)
        self.assertEqual(len(message.splitlines()), 1)

    def test_partitions_build_variants_without_counting_shared_tests_twice(self):
        enabled = {'caseid': 'enabled'}
        disabled = {'caseid': 'disabled', 'disabled': True, 'skip_reason': 'No runner'}
        projects = {'app': {'apps': [app('', [enabled, enabled, disabled]), app('off', [disabled]),
                                    app('compile'), app('unexplained'), app('not-built')], 'missing_apps': []}}
        built = {COVERAGE.app_key('app', 'esp32', c) for c in ['', 'off', 'compile', 'unexplained']}
        report = COVERAGE.classify(projects, built, {COVERAGE.app_key('app', 'esp32', 'compile'): 'Compile-only API example'}, {'esp32'})
        self.assertEqual({k: len(v) for k, v in report.items()},
                         dict(tested=1, disabled=1, build_only=1, unsupported=0, warnings=1, errors=0))
        self.assertEqual(report['tested'][0]['config'], 'default')
        self.assertEqual(report['disabled'][0]['reasons'], ['No runner'])

    def test_untested_chip_variants_are_silent_build_only_coverage(self):
        enabled = {'caseid': 'enabled'}
        projects = {'app': {'apps': [app('', [enabled]), app('', target='esp32c2')], 'missing_apps': []}}
        built = {COVERAGE.app_key('app', 'esp32', 'default'), COVERAGE.app_key('app', 'esp32c2', 'default')}
        report = COVERAGE.classify(projects, built, {}, {'esp32', 'esp32c2'})
        self.assertEqual(report['warnings'], [])
        self.assertEqual(report['build_only'], [dict(app='app', target='esp32c2', config='default',
                                                     reason='no test case declares this variant')])

    def test_enabled_missing_build_is_error_even_when_app_is_declared_build_only(self):
        projects = {'missing': {'apps': [], 'missing_apps': [
            app('named', [{'caseid': 'active'}, {'caseid': 'inactive', 'disabled': True}]),
            app('off', [{'caseid': 'disabled', 'disabled': True}]),
        ]}}
        report = COVERAGE.classify(projects, set(), {COVERAGE.app_key('missing', 'esp32', 'named'): 'Build only'}, {'esp32'})
        self.assertEqual(report['errors'], [dict(app='missing', target='esp32', config='named', tests=['active'])])
        self.assertEqual(report['warnings'], [])

    def test_new_sdk_target_is_reported_without_hiding_missing_known_target_configs(self):
        projects = {'benchmark': {'apps': [app('default')], 'missing_apps': [
            app('default', [{'caseid': 'future-target'}], target='esp32s31'),
            app('typo', [{'caseid': 'bad-config'}]),
            app('typo', [{'caseid': 'host-bad-config'}], target='linux'),
            app('typo', [{'caseid': 'preview-bad-config'}], target='esp32p4'),
        ]}, 'nonexistent': {'apps': [], 'missing_apps': [
            app('default', [{'caseid': 'bad-app-path'}], target='esp32s31'),
        ]}}
        known = {'esp32', 'esp32p4', 'linux'}
        report = COVERAGE.classify(projects, set(), {}, known)
        self.assertEqual([row['tests'] for row in report['unsupported']], [['future-target']])
        self.assertEqual([row['tests'] for row in report['errors']],
                         [['bad-config'], ['host-bad-config'], ['preview-bad-config'], ['bad-app-path']])
        # As soon as the SDK knows the target, its missing config is an error.
        report = COVERAGE.classify(projects, set(), {}, known | {'esp32s31'})
        self.assertEqual(report['unsupported'], [])
        self.assertEqual(len(report['errors']), 5)

    def test_build_only_declaration_is_exact_and_supports_idf_conditions(self):
        keys = {COVERAGE.app_key('app', 'esp32', c) for c in ['default', 'special']}
        keys.add(COVERAGE.app_key('app/child', 'esp32', 'default'))
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'build-only.json'
            self.assertEqual(COVERAGE.read_build_only(path, keys), {})
            path.write_text(json.dumps([dict(app='app', condition='CONFIG_NAME == "special"', reason='API example')]))
            parser = lambda _: SimpleNamespace(get_value=lambda target, config: config == 'special')
            with patch.dict('sys.modules', {'esp_bool_parser': SimpleNamespace(parse_bool_expr=parser)}):
                self.assertEqual(COVERAGE.read_build_only(path, keys),
                                 {COVERAGE.app_key('app', 'esp32', 'special'): 'API example'})

    def test_build_only_declarations_reject_unknown_apps_and_missing_reasons(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'build-only.json'
            for declarations in [{}, [dict(app='unknown', reason='Typo')], [dict(app='app')]]:
                path.write_text(json.dumps(declarations))
                with self.subTest(declarations=declarations), \
                        patch.dict('sys.modules', {'esp_bool_parser': SimpleNamespace(parse_bool_expr=None)}), \
                        self.assertRaises(ValueError):
                    COVERAGE.read_build_only(path, {COVERAGE.app_key('app', 'esp32', 'default')})


if __name__ == '__main__':
    unittest.main()
