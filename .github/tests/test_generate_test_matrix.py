"""Post-build planning must never schedule tests for unavailable variants."""

import importlib.util
import json
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

SCRIPT = Path(__file__).resolve().parents[1] / 'actions/ci-tools/generate_test_matrix.py'
SPEC = importlib.util.spec_from_file_location('generate_test_matrix', SCRIPT)
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)


def group(target, markers, nodes=None):
    return {'targets': [target], 'markers': sorted(markers),
            'runner_tags': sorted({target, *markers}),
            'nodes': nodes if nodes is not None else [f'app/pytest_app.py::test[{target}]']}


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
        return GENERATOR.generate_test_matrix(groups)['include']

    def test_empty_groups_do_not_create_jobs(self):
        self.assertEqual(self.matrix(), [])
        self.assertEqual(self.matrix(group('esp32', ['generic'], [])), [])

    def test_dynamic_target_and_combined_markers_keep_exact_nodes(self):
        nodes = ['app/pytest_app.py::test[with spaces]', 'app/pytest_app.py::test[$(not-a-command)]']
        job, = self.matrix(group('esp32c5', ['generic', 'quad_psram'], nodes))
        self.assertEqual(job['idf_target'], 'esp32c5')
        self.assertEqual(job['runner_labels'], ['self-hosted', 'linux', 'docker', 'esp32c5', 'quad_psram'])
        self.assertEqual(job['nodes'], sorted(nodes))

    def test_runner_aliases_preserve_extra_requirements(self):
        ethernet, = self.matrix(group('esp32', ['ethernet', 'quad_psram']))
        self.assertEqual(ethernet['runner_labels'], ['self-hosted', 'linux', 'docker', 'ESP32-ETHERNET-KIT', 'quad_psram'])
        nand, = self.matrix(group('esp32', ['spi_nand_flash']))
        self.assertEqual(nand['runner_labels'], ['self-hosted', 'linux', 'docker', 'spi_nand_flash'])

    def test_qemu_and_linux_keep_their_execution_environments(self):
        qemu, = self.matrix(group('esp32s3', ['qemu']))
        linux, = self.matrix(group('linux', ['host_test']))
        self.assertEqual(qemu['runner_labels'], ['self-hosted', 'linux', 'docker'])
        self.assertTrue(qemu['setup_qemu'])
        self.assertEqual(qemu['pytest_args'], '--embedded-services idf,qemu')
        self.assertEqual(linux['runner_labels'], ['ubuntu-latest'])
        self.assertEqual(linux['pytest_args'], '--embedded-services idf')
        self.assertFalse(linux['setup_qemu'])
        self.assertEqual(self.matrix(group('esp32', ['qemu'])), [])

    def test_duplicate_groups_merge_with_stable_identity(self):
        groups = [group('esp32', ['generic'], ['b::test']), group('esp32', ['quad_psram']),
                  group('esp32', ['generic'], ['a::test'])]
        first = self.matrix(*groups)
        self.assertEqual(first, self.matrix(*reversed(groups)))
        self.assertEqual(len({job['id'] for job in first}), 2)
        self.assertEqual(next(job['nodes'] for job in first if job['marker'] == 'generic'), ['a::test', 'b::test'])

    def test_multi_dut_is_rejected_instead_of_split(self):
        multi = group('esp32', ['generic'])
        multi['targets'] = ['esp32', 'esp32']
        with self.assertRaisesRegex(ValueError, 'Multi-DUT'):
            self.matrix(multi)

    def test_collection_keeps_runtime_linux_ids_and_qemu_identity(self):
        linux = case(target='linux', markers=(), host=True, node='test.py::host[linux]')
        runtime_linux = case(target='linux', markers=(), host=True, node='test.py::host[linux-idf]')
        qemu = case(target='esp32s3', markers=('qemu',), host=True, emulator='qemu')
        collect = Mock(side_effect=[[linux, qemu], [runtime_linux]])
        with patch.dict('sys.modules', {'idf_ci': SimpleNamespace(get_pytest_cases=collect)}):
            groups = GENERATOR.collect_test_groups({key(target='linux'), key(target='esp32s3')})
        self.assertEqual(collect.call_args_list[1].kwargs['target'], 'linux')
        self.assertEqual(next(g['nodes'] for g in groups if g['targets'] == ['linux']), [runtime_linux.item.nodeid])
        self.assertEqual(next(g['markers'] for g in groups if 'qemu' in g['markers']), ['qemu'])

    def test_test_requires_every_referenced_application(self):
        multi_app = case()
        multi_app.apps.append(SimpleNamespace(path='other', target='esp32', config='special'))
        with patch.dict('sys.modules', {'idf_ci': SimpleNamespace(get_pytest_cases=lambda **kwargs: [multi_app])}):
            self.assertEqual(GENERATOR.collect_test_groups({key()}), [])
            self.assertEqual(len(GENERATOR.collect_test_groups({key(), key('other', config='special')})), 1)

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
        first = self.metadata('build_info_esp32_1.json', [self.record('success'), self.record('skipped', 'psram')])
        second = self.metadata('build_info_esp32_2.json', [self.record('failed', path='other'),
                                                         self.record('success', 'special', path='other')])
        self.assertEqual(GENERATOR.successful_apps([first, second]), {key(), key('other', config='special')})

    def test_all_skipped_or_empty_shards_produce_no_test_jobs(self):
        skipped = self.metadata('build_info_esp32_1.json', [self.record('skipped')])
        empty = self.metadata('build_info_esp32_2.json', [])
        with patch.object(GENERATOR, 'collect_eligible_apps') as collect:
            self.assertEqual(GENERATOR.plan_tests([skipped, empty]), {'include': []})
            collect.assert_not_called()

    def test_mixed_results_only_schedule_matching_configurations(self):
        info = self.metadata('build_info_esp32_1.json', [self.record('success'), self.record('skipped', 'psram')])
        cases = [case(), case(config='psram', markers=('quad_psram',)), case(path='absent')]
        with patch.object(GENERATOR, 'collect_eligible_apps', side_effect=lambda built: built), \
                patch.dict('sys.modules', {'idf_ci': SimpleNamespace(get_pytest_cases=lambda **kwargs: cases)}):
            matrix = GENERATOR.plan_tests([info])
        job, = matrix['include']
        self.assertEqual(job['nodes'], [cases[0].item.nodeid])
        self.assertEqual(job['marker'], 'generic')

    def test_successful_builds_without_tests_create_no_jobs(self):
        info = self.metadata('build_info_esp32_1.json', [self.record('success')])
        with patch.object(GENERATOR, 'collect_eligible_apps', side_effect=lambda built: built), \
                patch.dict('sys.modules', {'idf_ci': SimpleNamespace(get_pytest_cases=lambda **kwargs: [])}):
            self.assertEqual(GENERATOR.plan_tests([info]), {'include': []})

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
                patch('sys.argv', ['planner', str(self.root)]):
            GENERATOR.main()
        values = dict(line.split('=', 1) for line in output.read_text().splitlines())
        self.assertEqual(json.loads(values['test_matrix']), {'include': []})
        self.assertEqual(values['has_tests'], 'false')


if __name__ == '__main__':
    unittest.main()
