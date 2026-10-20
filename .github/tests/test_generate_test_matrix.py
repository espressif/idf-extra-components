"""Post-build planning must never schedule tests for unavailable variants."""

import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

SCRIPT = Path(__file__).resolve().parents[1] / 'actions/ci-tools/generate_test_matrix.py'
SPEC = importlib.util.spec_from_file_location('generate_test_matrix', SCRIPT)
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)


def pytest_app(path, target, config):
    return SimpleNamespace(build_dir=str(Path(path).resolve() / f'build_{target}_{config or "default"}'))


def case(path='app', target='esp32', config='default', markers=('generic',), host=False, emulator=None, node=None):
    result = SimpleNamespace(
        targets=[target], env_markers=set(markers), runner_tags=tuple(sorted({target, *markers})),
        env_selector=','.join(sorted(markers)),
        is_host_test=host, emulator_marker=emulator,
        apps=[SimpleNamespace(path=path, target=target, config=config)],
        item=SimpleNamespace(nodeid=node or f'{path}/pytest_app.py::test[{target}-{config}]'),
    )

    result.get_skip_reason_if_not_built = Mock(side_effect=lambda dirs: None if all(
        pytest_app(a.path, a.target, a.config).build_dir in dirs for a in result.apps
    ) else 'Missing binary')
    return result


def key(path='app', target='esp32', config='default'):
    return pytest_app(path, target, config).build_dir


class TestMatrixTests(unittest.TestCase):
    def matrix(self, *groups):
        # Test only the GitHub adapter; grouping itself belongs to idf-ci.
        return GENERATOR.generate_test_matrix([
            group if isinstance(group, list) else [group] for group in groups
        ])['include']

    def test_empty_collection_does_not_create_jobs(self):
        self.assertEqual(self.matrix(), [])
        with patch.dict('sys.modules', {'idf_ci': SimpleNamespace(PytestApp=pytest_app, get_pytest_cases=Mock())}):
            self.assertEqual(GENERATOR.collect_test_cases(set()), [])

    def test_dynamic_target_and_combined_markers_keep_exact_nodes(self):
        nodes = ['app/pytest_app.py::test[with spaces]', 'app/pytest_app.py::test[$(not-a-command)]']
        job, = self.matrix([case(target='esp32c5', markers=['generic', 'quad_psram'], node=node) for node in nodes])
        self.assertEqual(job['idf_target'], 'esp32c5')
        self.assertEqual(job['runner_labels'], ['self-hosted', 'linux', 'docker', 'quad_psram'])
        self.assertEqual(job['execution'], 'hardware')
        self.assertEqual(job['nodes'], '\n'.join(sorted(nodes)))

    def test_environment_markers_are_runner_labels_and_preserve_target(self):
        generic, = self.matrix(case())
        self.assertEqual(generic['runner_labels'], ['self-hosted', 'linux', 'docker', 'esp32'])
        ethernet, = self.matrix(case(markers=['ethernet', 'quad_psram']))
        self.assertEqual(ethernet['runner_labels'], ['self-hosted', 'linux', 'docker', 'ESP32-ETHERNET-KIT', 'quad_psram'])
        nand, = self.matrix(case(markers=['spi_nand_flash']))
        self.assertEqual(nand['runner_labels'], ['self-hosted', 'linux', 'docker', 'spi_nand_flash'])

    def test_unmapped_environment_marker_fails_loudly(self):
        with self.assertRaisesRegex(ValueError, 'unknown_marker'):
            self.matrix(case(markers=['unknown_marker']))

    def test_execution_profiles_use_native_case_metadata(self):
        for target in ('esp32s3', 'esp32c3'):
            qemu, = self.matrix(case(target=target, markers=['qemu'], host=True, emulator='qemu'))
            self.assertEqual(qemu['execution'], 'qemu')
            self.assertEqual(qemu['runner_labels'], ['ubuntu-24.04'])
            hardware, = self.matrix(case(target=target))
            self.assertEqual(hardware['execution'], 'hardware')
            self.assertEqual(hardware['runner_labels'], ['self-hosted', 'linux', 'docker', target])
            for job in (qemu, hardware):
                self.assertTrue({'runner_tags', 'pytest_args', 'setup_qemu'}.isdisjoint(job))
        linux, = self.matrix(case(target='linux', markers=[], host=True))
        self.assertEqual(linux['execution'], 'linux')
        self.assertEqual(linux['runner_labels'], ['ubuntu-24.04'])
        self.assertEqual(linux['marker'], 'host_test')

    def test_native_group_order_is_preserved_and_nodes_are_unique(self):
        generic = [case(node='b::test'), case(node='a::test'), case(node='b::test')]
        psram = case(markers=['quad_psram'])
        first = self.matrix(generic, psram)
        self.assertEqual(first, list(reversed(self.matrix(psram, list(reversed(generic))))))
        self.assertEqual(next(job['nodes'] for job in first if job['marker'] == 'generic'), 'a::test\nb::test')

    def test_collected_environments_need_no_central_allowlist(self):
        candidates = [case(target='esp32c2'), case(target='esp32c2', markers=['generic', 'quad_psram']),
                      case(target='esp32c3')]
        result = self.matrix(*candidates)
        self.assertEqual({(j['idf_target'], j['marker']) for j in result},
                         {(c.targets[0], c.env_selector) for c in candidates})

    def test_unknown_execution_profile_fails_planning(self):
        with self.assertRaisesRegex(ValueError, 'Unsupported test execution profile: espemu'):
            self.matrix(case(markers=['espemu'], emulator='espemu'))

    def test_qemu_target_support_is_validated_before_setup(self):
        qemu, = self.matrix(case(target='esp32', markers=['qemu'], emulator='qemu'))
        self.assertEqual(qemu['execution'], 'qemu')
        result = subprocess.run(['bash', str(SCRIPT.with_name('setup_qemu.sh'))],
                                env={**os.environ, 'IDF_TARGET_NAME': 'esp32'}, capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn('does not support target esp32', result.stderr)

    def test_multi_dut_is_rejected_instead_of_split(self):
        multi = case()
        multi.targets = ['esp32', 'esp32']
        with self.assertRaisesRegex(ValueError, 'Multi-DUT'):
            self.matrix(multi)

    def test_linux_and_qemu_are_collected_in_one_pass(self):
        linux = case(target='linux', markers=(), host=True, node='test.py::host[linux]')
        qemu = case(target='esp32s3', markers=('qemu',), host=True, emulator='qemu')
        collect = Mock(return_value=[linux, qemu])
        with patch.dict('sys.modules', {'idf_ci': SimpleNamespace(PytestApp=pytest_app, get_pytest_cases=collect)}):
            cases = GENERATOR.collect_test_cases({key(target='linux'), key(target='esp32s3')})
        collect.assert_called_once_with(target='all', marker_expr='', additional_args=['--suppress-no-test-exit-code'])
        self.assertEqual(cases, [linux, qemu])

    def test_collection_keeps_multi_dut_cases_for_explicit_worker_validation(self):
        multi = case()
        multi.targets = ['esp32', 'esp32']
        multi.apps.append(SimpleNamespace(path='app', target='esp32', config='default'))
        collect = Mock(return_value=[multi])
        with patch.dict('sys.modules', {'idf_ci': SimpleNamespace(PytestApp=pytest_app, get_pytest_cases=collect)}):
            cases = GENERATOR.collect_test_cases({key()})
        self.assertEqual(collect.call_args.kwargs['target'], 'all')
        with self.assertRaisesRegex(ValueError, 'Multi-DUT'):
            self.matrix(cases)

    def test_test_requires_every_referenced_application(self):
        multi_app = case()
        multi_app.apps.append(SimpleNamespace(path='other', target='esp32', config='special'))
        with patch.dict('sys.modules', {'idf_ci': SimpleNamespace(PytestApp=pytest_app, get_pytest_cases=lambda **kwargs: [multi_app])}):
            self.assertEqual(GENERATOR.collect_test_cases({key()}), [])
            self.assertEqual(GENERATOR.collect_test_cases({key(), key('other', config='special')}), [multi_app])


class BuildResultsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        # Mock the SDK API so hosted unit checks need no installed ESP-IDF.
        def read(files):
            return [SimpleNamespace(**json.loads(line)) for file in files
                    for line in file.read_text().splitlines()]
        native = patch.dict('sys.modules', {
            'idf_ci': SimpleNamespace(PytestApp=pytest_app),
            'idf_ci.settings': SimpleNamespace(CiSettings=SimpleNamespace(read_apps_from_files=read)),
            'idf_build_apps.constants': SimpleNamespace(BuildStatus=SimpleNamespace(SUCCESS='build success')),
        })
        native.start()
        self.addCleanup(native.stop)

    def metadata(self, name, records):
        path = self.root / name
        path.write_text(''.join(json.dumps(record) + '\n' for record in records))
        return path

    def record(self, status, config='', path='app', target='esp32', test_comment=None):
        return dict(app_dir=path, target=target, config_name=config, build_status=status, test_comment=test_comment)

    def test_only_successful_exact_variants_survive_across_shards(self):
        first = self.metadata('build_info_esp32_1.json', [self.record('build success'), self.record('skipped', 'psram')])
        second = self.metadata('build_info_esp32_2.json', [self.record('build failed', path='other'),
                                                         self.record('build success', 'special', path='other')])
        self.assertEqual(GENERATOR.eligible_apps([first, second]), {key(), key('other', config='special')})

    def test_build_manifest_exclusion_is_read_from_metadata_per_variant(self):
        info = self.metadata('build_info_linux_1.json', [
            self.record('build success', target='linux', path='unity', test_comment='Disabled by manifest'),
            self.record('build success', target='linux', path='example'),
            self.record('build success', target='linux', path='example', config='special', test_comment='Not enabled'),
        ])
        self.assertEqual(GENERATOR.eligible_apps([info]), {key('example', 'linux')})

    def test_all_skipped_or_empty_shards_produce_no_test_jobs(self):
        skipped = self.metadata('build_info_esp32_1.json', [self.record('skipped')])
        empty = self.metadata('build_info_esp32_2.json', [])
        with patch.object(GENERATOR, 'collect_test_cases') as collect:
            self.assertEqual(GENERATOR.plan_tests([skipped, empty]), {'include': []})
            collect.assert_not_called()

    def test_mixed_results_only_schedule_matching_configurations(self):
        info = self.metadata('build_info_esp32_1.json', [self.record('build success'), self.record('skipped', 'psram')])
        cases = [case(), case(config='psram', markers=('quad_psram',)), case(path='absent')]
        group = Mock(return_value=SimpleNamespace(grouped_cases={'hardware': [cases[0]]}))
        with patch.dict('sys.modules', {
            'idf_ci': SimpleNamespace(PytestApp=pytest_app, get_pytest_cases=lambda **kwargs: cases),
            'idf_ci.idf_pytest': SimpleNamespace(GroupedPytestCases=group),
        }):
            matrix = GENERATOR.plan_tests([info])
        group.assert_called_once_with([cases[0]])
        job, = matrix['include']
        self.assertEqual(job['nodes'], cases[0].item.nodeid)
        self.assertEqual(job['marker'], 'generic')

    def test_successful_builds_without_tests_create_no_jobs(self):
        info = self.metadata('build_info_esp32_1.json', [self.record('build success')])
        group = Mock(return_value=SimpleNamespace(grouped_cases={}))
        with patch.dict('sys.modules', {
            'idf_ci': SimpleNamespace(PytestApp=pytest_app, get_pytest_cases=lambda **kwargs: []),
            'idf_ci.idf_pytest': SimpleNamespace(GroupedPytestCases=group),
        }):
            self.assertEqual(GENERATOR.plan_tests([info]), {'include': []})
        group.assert_called_once_with([])

    def test_missing_build_metadata_is_an_error_not_an_empty_matrix(self):
        build_matrix = {'include': [{'idf_target': 'esp32', 'parallel_index': 1}]}
        with patch.dict(os.environ, {'BUILD_MATRIX': json.dumps(build_matrix)}), \
                patch('sys.argv', ['planner', str(self.root)]):
            with self.assertRaisesRegex(SystemExit, 'missing=.*build_info_esp32_1.json'):
                GENERATOR.main()

    def test_cli_publishes_empty_matrix_and_false_guard_after_skipped_build(self):
        self.metadata('build_info_esp32_1.json', [self.record('skipped')])
        build_matrix = {'include': [{'idf_target': 'esp32', 'parallel_index': 1}]}
        output = self.root / 'outputs'
        with patch.dict(os.environ, {'BUILD_MATRIX': json.dumps(build_matrix), 'GITHUB_OUTPUT': str(output)}), \
                patch('sys.argv', ['planner', str(self.root), '--selection-report', str(self.root / 'selection.json')]):
            GENERATOR.main()
        values = dict(line.split('=', 1) for line in output.read_text().splitlines())
        self.assertEqual(json.loads(values['test_matrix']), {'include': []})
        self.assertEqual(values['has_tests'], 'false')
        self.assertEqual(json.loads((self.root / 'selection.json').read_text()), {'include': []})


if __name__ == '__main__':
    unittest.main()
