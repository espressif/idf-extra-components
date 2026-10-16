"""Post-build planning must never schedule tests for unavailable variants."""

import importlib.util
import json
import os
from pathlib import Path
import shlex
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

SCRIPT = Path(__file__).resolve().parents[1] / 'actions/ci-tools/generate_test_matrix.py'
SPEC = importlib.util.spec_from_file_location('generate_test_matrix', SCRIPT)
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)


def case(path='app', target='esp32', config='default', markers=('generic',), host=False, emulator=None, node=None):
    return SimpleNamespace(
        targets=[target], env_markers=set(markers), runner_tags=tuple(sorted({target, *markers})),
        is_host_test=host, emulator_marker=emulator,
        apps=[SimpleNamespace(path=path, target=target, config=config)],
        item=SimpleNamespace(nodeid=node or f'{path}/pytest_app.py::test[{target}-{config}]'),
    )


def key(path='app', target='esp32', config='default'):
    return str(Path(path).resolve()), target, config


class TestMatrixTests(unittest.TestCase):
    def matrix(self, *groups):
        # Test only the GitHub adapter; grouping itself belongs to idf-ci.
        return GENERATOR.generate_test_matrix([
            group if isinstance(group, list) else [group] for group in groups
        ])['include']

    def test_empty_collection_does_not_create_jobs(self):
        self.assertEqual(self.matrix(), [])
        with patch.dict('sys.modules', {'idf_ci': SimpleNamespace(get_pytest_cases=Mock())}):
            self.assertEqual(GENERATOR.collect_test_cases(set()), [])

    def test_dynamic_target_and_combined_markers_keep_exact_nodes(self):
        nodes = ['app/pytest_app.py::test[with spaces]', 'app/pytest_app.py::test[$(not-a-command)]']
        job, = self.matrix([case(target='esp32c5', markers=['generic', 'quad_psram'], node=node) for node in nodes])
        self.assertEqual(job['idf_target'], 'esp32c5')
        self.assertEqual(job['runner_labels'], ['self-hosted', 'linux', 'docker', 'esp32c5', 'quad_psram'])
        self.assertEqual(job['nodes'], sorted(nodes))

    def test_runner_aliases_preserve_extra_requirements(self):
        ethernet, = self.matrix(case(markers=['ethernet', 'quad_psram']))
        self.assertEqual(ethernet['runner_labels'], ['self-hosted', 'linux', 'docker', 'ESP32-ETHERNET-KIT', 'quad_psram'])
        nand, = self.matrix(case(markers=['spi_nand_flash']))
        self.assertEqual(nand['runner_labels'], ['self-hosted', 'linux', 'docker', 'spi_nand_flash'])

    def test_qemu_and_linux_keep_their_execution_environments(self):
        for target in ('esp32s3', 'esp32c3'):
            qemu, = self.matrix(case(target=target, markers=['qemu'], host=True, emulator='qemu'))
            self.assertEqual(qemu['runner_labels'], ['self-hosted', 'linux', 'docker'])
            self.assertTrue(qemu['setup_qemu'])
            self.assertEqual(shlex.split(qemu['pytest_args']), [
                '--embedded-services', 'idf,qemu',
            ])
            hardware, = self.matrix(case(target=target))
            self.assertEqual(hardware['pytest_args'], '')
        linux, = self.matrix(case(target='linux', markers=[], host=True))
        self.assertEqual(linux['runner_labels'], ['ubuntu-24.04'])
        self.assertEqual(linux['pytest_args'], '--embedded-services idf')
        self.assertEqual(linux['markers'], ['host_test'])
        self.assertFalse(linux['setup_qemu'])

    def test_group_order_and_duplicate_nodes_do_not_change_identity(self):
        generic = [case(node='b::test'), case(node='a::test'), case(node='b::test')]
        psram = case(markers=['quad_psram'])
        first = self.matrix(generic, psram)
        self.assertEqual(first, self.matrix(psram, list(reversed(generic))))
        self.assertEqual(len({job['id'] for job in first}), 2)
        self.assertEqual(next(job['nodes'] for job in first if job['marker'] == 'generic'), ['a::test', 'b::test'])

    def test_collected_environments_need_no_central_allowlist(self):
        candidates = [case(target='esp32c2'), case(target='esp32c2', markers=['generic', 'quad_psram']),
                      case(target='esp32c3')]
        result = self.matrix(*candidates)
        self.assertEqual({(j['idf_target'], tuple(j['markers'])) for j in result},
                         {(c.targets[0], tuple(sorted(c.env_markers))) for c in candidates})

    def test_unsupported_worker_fails_instead_of_silently_losing_tests(self):
        with self.assertRaisesRegex(ValueError, 'QEMU worker does not support target esp32'):
            self.matrix(case(markers=['qemu']))

    def test_multi_dut_is_rejected_instead_of_split(self):
        multi = case()
        multi.targets = ['esp32', 'esp32']
        with self.assertRaisesRegex(ValueError, 'Multi-DUT'):
            self.matrix(multi)

    def test_collection_uses_exact_linux_runtime_ids(self):
        generic_linux = case(target='linux', markers=(), host=True, node='test.py::host[linux]')
        linux = case(target='linux', markers=(), host=True, node='test.py::host[linux-idf]')
        qemu = case(target='esp32s3', markers=('qemu',), host=True, emulator='qemu')
        collect = Mock(side_effect=[[generic_linux, qemu], [linux]])
        with patch.dict('sys.modules', {'idf_ci': SimpleNamespace(get_pytest_cases=collect)}):
            cases = GENERATOR.collect_test_cases({key(target='linux'), key(target='esp32s3')})
        self.assertNotIn('target', collect.call_args_list[0].kwargs)
        self.assertEqual(collect.call_args_list[1].kwargs['target'], 'linux')
        self.assertEqual(cases, [qemu, linux])

    def test_collection_keeps_multi_dut_cases_for_explicit_worker_validation(self):
        multi = case()
        multi.targets = ['esp32', 'esp32']
        multi.apps.append(SimpleNamespace(path='app', target='esp32', config='default'))
        collect = Mock(return_value=[multi])
        with patch.dict('sys.modules', {'idf_ci': SimpleNamespace(get_pytest_cases=collect)}):
            cases = GENERATOR.collect_test_cases({key()})
        self.assertNotIn('target', collect.call_args.kwargs)
        with self.assertRaisesRegex(ValueError, 'Multi-DUT'):
            self.matrix(cases)

    def test_test_requires_every_referenced_application(self):
        multi_app = case()
        multi_app.apps.append(SimpleNamespace(path='other', target='esp32', config='special'))
        with patch.dict('sys.modules', {'idf_ci': SimpleNamespace(get_pytest_cases=lambda **kwargs: [multi_app])}):
            self.assertEqual(GENERATOR.collect_test_cases({key()}), [])
            self.assertEqual(GENERATOR.collect_test_cases({key(), key('other', config='special')}), [multi_app])

    def test_manifest_restriction_does_not_disable_sibling_apps_or_configs(self):
        apps = {name: SimpleNamespace(app_dir=path, target='linux', config_name=config)
                for name, path, config in [('unity', 'unity', ''), ('example', 'example', ''),
                                           ('special', 'example', 'special')]}
        api = SimpleNamespace(
            collect_build_apps=Mock(return_value=(apps, {})),
            enabled_test_targets=lambda app: ['linux'] if app.app_dir == 'example' and not app.config_name else [],
        )
        with patch.dict('sys.modules', {'idf_ci.build_collect.scripts': api}):
            eligible = GENERATOR.collect_eligible_apps({key('unity', 'linux'), key('example', 'linux'),
                                                        key('example', 'linux', 'special')})
        self.assertEqual(eligible, {key('example', 'linux')})


class BuildResultsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def metadata(self, name, records):
        path = self.root / name
        path.write_text(''.join(json.dumps(record) + '\n' for record in records))
        return path

    def record(self, status, config='', path='app', target='esp32'):
        return dict(app_dir=path, target=target, config_name=config, build_status=status)

    def test_only_successful_exact_variants_survive_across_shards(self):
        first = self.metadata('build_info_esp32_1.json', [self.record('build success'), self.record('skipped', 'psram')])
        second = self.metadata('build_info_esp32_2.json', [self.record('build failed', path='other'),
                                                         self.record('build success', 'special', path='other')])
        self.assertEqual(GENERATOR.successful_apps([first, second]), {key(), key('other', config='special')})

    def test_all_skipped_or_empty_shards_produce_no_test_jobs(self):
        skipped = self.metadata('build_info_esp32_1.json', [self.record('skipped')])
        empty = self.metadata('build_info_esp32_2.json', [])
        with patch.object(GENERATOR, 'collect_eligible_apps') as collect:
            self.assertEqual(GENERATOR.plan_tests([skipped, empty]), {'include': []})
            collect.assert_not_called()

    def test_mixed_results_only_schedule_matching_configurations(self):
        info = self.metadata('build_info_esp32_1.json', [self.record('build success'), self.record('skipped', 'psram')])
        cases = [case(), case(config='psram', markers=('quad_psram',)), case(path='absent')]
        group = Mock(return_value=SimpleNamespace(grouped_cases={'hardware': [cases[0]]}))
        with patch.object(GENERATOR, 'collect_eligible_apps', side_effect=lambda built: built), \
                patch.dict('sys.modules', {
                    'idf_ci': SimpleNamespace(get_pytest_cases=lambda **kwargs: cases),
                    'idf_ci.idf_pytest': SimpleNamespace(GroupedPytestCases=group),
                }):
            matrix = GENERATOR.plan_tests([info])
        group.assert_called_once_with([cases[0]])
        job, = matrix['include']
        self.assertEqual(job['nodes'], [cases[0].item.nodeid])
        self.assertEqual(job['marker'], 'generic')

    def test_successful_builds_without_tests_create_no_jobs(self):
        info = self.metadata('build_info_esp32_1.json', [self.record('build success')])
        group = Mock(return_value=SimpleNamespace(grouped_cases={}))
        with patch.object(GENERATOR, 'collect_eligible_apps', side_effect=lambda built: built), \
                patch.dict('sys.modules', {
                    'idf_ci': SimpleNamespace(get_pytest_cases=lambda **kwargs: []),
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
