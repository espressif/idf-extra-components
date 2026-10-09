"""Regression coverage for per-environment planning without ESP-IDF."""

import copy
import importlib.util
import unittest
from collections import Counter
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
from unittest.mock import Mock

SCRIPT = Path(__file__).resolve().parents[1] / 'actions/ci-tools/generate_build_matrix.py'
SPEC = importlib.util.spec_from_file_location('generate_build_matrix', SCRIPT)
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)


def group(target, markers, nodes=None, tags=None):
    return {'targets': [target], 'markers': sorted(markers),
            'runner_tags': sorted(tags if tags is not None else {target, *markers}),
            'nodes': nodes or [f'app/pytest_app.py::test[{target}]']}


class GenerateMatricesTests(unittest.TestCase):
    def generate(self, counts=None, markers=None):
        groups = ([group(target, [marker]) for target, markers_for_target in markers.items()
                   for marker in markers_for_target] if isinstance(markers, dict) else markers or [])
        with patch.object(GENERATOR, 'MAX_BUILD_SHARDS', 3), patch.object(GENERATOR, 'APPS_PER_SHARD', 10):
            return GENERATOR.generate_matrices(Counter(counts or {}), groups)

    def app(self, matrices, target):
        return next(entry for entry in matrices['apps_matrix']['include'] if entry['idf_target'] == target)

    def test_each_environment_has_its_own_exact_plan(self):
        # The second environment has removed esp32 apps and introduced esp32s3.
        # Neither invocation is allowed to borrow targets/shards from the other.
        first = self.generate({'esp32': 11}, {'esp32': {'generic'}})
        second = self.generate({'esp32s3': 1}, {'esp32s3': {'qemu'}})
        self.assertEqual([a['idf_target'] for a in first['apps_matrix']['include']], ['esp32'])
        self.assertEqual([a['idf_target'] for a in second['apps_matrix']['include']], ['esp32s3'])
        self.assertEqual(self.app(first, 'esp32')['parallel_count'], 2)
        self.assertEqual(self.app(second, 'esp32s3')['parallel_count'], 1)
        self.assertEqual(self.app(second, 'esp32s3')['tests'][0]['marker'], 'qemu')

    def test_new_markers_are_discovered_without_configuration(self):
        tests = self.app(self.generate({'esp32': 1}, {'esp32': {'generic', 'quad_psram'}}), 'esp32')['tests']
        self.assertEqual({test['marker'] for test in tests}, {'generic', 'quad_psram'})
        psram = next(test for test in tests if test['marker'] == 'quad_psram')
        self.assertEqual(psram['runner_labels'], ['self-hosted', 'linux', 'docker', 'esp32', 'quad_psram'])

    def test_new_target_gets_a_worker_and_labels_from_pytest(self):
        nodes = ['new_app/pytest_app.py::test[esp32c5]']
        matrix = self.generate({'esp32c5': 11}, [group('esp32c5', ['generic', 'quad_psram'], nodes)])
        app = self.app(matrix, 'esp32c5')
        self.assertEqual(app['parallel_count'], 2)
        self.assertEqual(app['tests'][0]['nodes'], nodes)
        self.assertEqual(app['tests'][0]['runner_labels'], ['self-hosted', 'linux', 'docker', 'esp32c5', 'quad_psram'])
        self.assertEqual(matrix['extra_matrix']['include'], [])

    def test_dynamic_partition_keeps_all_builds_without_duplicates(self):
        counts = {'esp32c5': 4, 'esp32s3': 3, 'esp32h2': 2, 'linux': 1}
        matrix = self.generate(counts, [group('esp32c5', ['generic']), group('esp32s3', ['qemu'])])
        self.assertEqual([app['idf_target'] for app in matrix['apps_matrix']['include']], ['esp32c5', 'esp32s3', 'linux'])
        self.assertEqual(matrix['extra_matrix']['include'], [{'parallel_index': 1, 'parallel_count': 1}])

    def test_group_without_a_buildable_target_never_creates_a_worker(self):
        self.assertEqual(self.generate({'esp32': 1}, [group('esp32c5', ['generic'])])['apps_matrix']['include'], [])

    def test_combined_markers_stay_one_group_and_nodes_are_lossless(self):
        nodes = ['app/pytest_app.py::test[with spaces]', 'app/pytest_app.py::test[$(not-a-command)]']
        tests = self.app(self.generate({'esp32': 1}, [group('esp32', ['generic', 'quad_psram'], nodes)]), 'esp32')['tests']
        self.assertEqual(len(tests), 1)
        self.assertEqual(tests[0]['markers'], ['generic', 'quad_psram'])
        self.assertEqual(tests[0]['nodes'], sorted(nodes))
        self.assertEqual(tests[0]['runner_labels'], ['self-hosted', 'linux', 'docker', 'esp32', 'quad_psram'])

    def test_infrastructure_overrides_preserve_other_requirements(self):
        test = self.app(self.generate({'esp32': 1}, [group('esp32', ['ethernet', 'quad_psram'])]), 'esp32')['tests'][0]
        self.assertEqual(test['runner_labels'], ['self-hosted', 'linux', 'docker', 'ESP32-ETHERNET-KIT', 'quad_psram'])
        test = self.app(self.generate({'esp32': 1}, [group('esp32', ['spi_nand_flash'])]), 'esp32')['tests'][0]
        self.assertEqual(test['runner_labels'], ['self-hosted', 'linux', 'docker', 'spi_nand_flash'])

    def test_qemu_and_host_do_not_require_hardware_target_labels(self):
        matrices = self.generate({'esp32s3': 1, 'linux': 1}, [group('esp32s3', ['qemu']), group('linux', ['host_test'])])
        qemu = self.app(matrices, 'esp32s3')['tests'][0]
        host = self.app(matrices, 'linux')['tests'][0]
        self.assertEqual(qemu['runner_labels'], ['self-hosted', 'linux', 'docker'])
        self.assertTrue(qemu['setup_qemu'])
        self.assertEqual(qemu['pytest_args'], '--embedded-services idf,qemu')
        self.assertEqual(host['runner_labels'], ['ubuntu-latest'])
        self.assertFalse(host['setup_qemu'])

    def test_qemu_capability_limits_do_not_hide_new_hardware_markers(self):
        tests = self.app(self.generate({'esp32': 1}, [
            group('esp32', ['qemu']), group('esp32', ['quad_psram']),
        ]), 'esp32')['tests']
        self.assertEqual([test['marker'] for test in tests], ['quad_psram'])

    def test_multi_dut_is_rejected_instead_of_split_into_invalid_jobs(self):
        multi = group('esp32', ['generic'])
        multi['targets'] = ['esp32', 'esp32']
        with self.assertRaisesRegex(ValueError, 'Multi-DUT'):
            self.generate({'esp32': 1}, [multi])

    def test_empty_discovery_has_no_fallback_jobs(self):
        matrices = self.generate({}, {'esp32': {'generic'}})
        self.assertEqual(matrices, {'apps_matrix': {'include': []}, 'extra_matrix': {'include': []}})

    def test_shards_round_up_and_obey_cap(self):
        for count, expected in [(1, 1), (10, 1), (11, 2), (20, 2), (21, 3), (100, 3)]:
            with self.subTest(count=count):
                matrices = self.generate({'esp32': count, 'esp32c6': count}, {'esp32': ['generic']})
                app = self.app(matrices, 'esp32')
                self.assertEqual(app['parallel_count'], expected)
                self.assertEqual(app['parallel_indices'], list(range(1, expected + 1)))
                self.assertEqual(matrices['extra_matrix']['include'], [
                    {'parallel_index': i, 'parallel_count': expected} for i in range(1, expected + 1)
                ])

    def test_extra_counts_only_targets_outside_the_target_workers(self):
        matrices = self.generate({'esp32': 100, 'linux': 100, 'esp32c6': 6, 'esp32h2': 5}, {'esp32': ['generic']})
        self.assertEqual(len(matrices['extra_matrix']['include']), 2)
        self.assertTrue(all(e['parallel_count'] == 2 for e in matrices['extra_matrix']['include']))

    def test_no_cases_keeps_linux_separate_and_other_apps_compile_only(self):
        matrices = self.generate({'linux': 1, 'esp32': 1})
        app = self.app(matrices, 'linux')
        self.assertFalse(app['run_tests'])
        self.assertEqual(app['tests'], [])
        self.assertEqual([entry['idf_target'] for entry in matrices['apps_matrix']['include']], ['linux'])
        self.assertEqual(matrices['extra_matrix']['include'], [{'parallel_index': 1, 'parallel_count': 1}])

    def test_group_ids_are_stable_unique_and_duplicate_groups_merge(self):
        plain = group('esp32', ['generic'], ['a.py::test'])
        special = group('esp32', ['generic', 'quad_psram'], ['b.py::test'])
        repeated = group('esp32', ['generic'], ['c.py::test'])
        first = self.app(self.generate({'esp32': 1}, [plain, special, repeated]), 'esp32')['tests']
        second = self.app(self.generate({'esp32': 1}, [repeated, special, plain]), 'esp32')['tests']
        self.assertEqual(first, second)
        self.assertEqual(len({t['id'] for t in first}), 2)
        self.assertEqual(next(t['nodes'] for t in first if t['marker'] == 'generic'), ['a.py::test', 'c.py::test'])

    def test_generation_does_not_mutate_inputs(self):
        counts, markers = Counter(esp32=11), [group('esp32', ['generic'])]
        before = copy.deepcopy((counts, markers))
        first = GENERATOR.generate_matrices(counts, markers)
        second = GENERATOR.generate_matrices(counts, markers)
        self.assertEqual((counts, markers), before)
        self.assertEqual(first, second)

    def test_collection_counts_only_buildable_apps_and_keeps_emulators_and_host(self):
        build = {'projects': {'app': {'apps': [
            {'target': 'esp32', 'config': 'default', 'build_status': 'should be built'},
            {'target': 'esp32', 'config': 'default', 'build_status': 'disabled'},
            {'target': 'linux', 'config': 'default', 'build_status': 'should be built'},
        ]}}}
        groups = [group('esp32', ['generic']), group('esp32s3', ['qemu']), group('linux', ['host_test'])]
        with patch.object(GENERATOR, 'run_idf_ci', return_value=build), patch.object(GENERATOR, 'collect_test_groups', return_value=groups):
            counts, collected = GENERATOR.collect_discovery()
        self.assertEqual(counts, {'esp32': 1, 'linux': 1})
        self.assertEqual(collected, groups)

    def test_collection_keeps_runtime_linux_ids_and_does_not_exclude_qemu_as_host(self):
        def case(target, markers, node, host=False, emulator=None):
            return SimpleNamespace(targets=[target], env_markers=set(markers),
                                   runner_tags=tuple(sorted({target, *markers})),
                                   is_host_test=host, emulator_marker=emulator,
                                   apps=[SimpleNamespace(path='app', target=target, config='default')],
                                   item=SimpleNamespace(nodeid=node, iter_markers=lambda name: []))

        linux = case('linux', [], 'test.py::host[linux]', host=True)
        runtime_linux = case('linux', [], 'test.py::host[linux-idf]', host=True)
        qemu = case('esp32s3', ['qemu'], 'test.py::emulator[esp32s3-qemu]', host=True, emulator='qemu')
        psram = case('esp32s3', ['generic', 'quad_psram'], 'test.py::hardware[with spaces]')
        collect = Mock(side_effect=[[linux, qemu, psram], [runtime_linux]])
        with patch.dict('sys.modules', {'idf_ci': SimpleNamespace(get_pytest_cases=collect)}):
            groups = GENERATOR.collect_test_groups({(str(Path('app').resolve()), t, 'default') for t in ['linux', 'esp32s3']})
        self.assertEqual(collect.call_args_list[1].kwargs['target'], 'linux')
        self.assertEqual(next(g['nodes'] for g in groups if g['targets'] == ['linux']), [runtime_linux.item.nodeid])
        self.assertEqual(next(g['markers'] for g in groups if 'qemu' in g['markers']), ['qemu'])
        self.assertEqual(next(g['markers'] for g in groups if 'quad_psram' in g['markers']), ['generic', 'quad_psram'])

    def test_unavailable_app_cannot_create_test_jobs(self):
        case = SimpleNamespace(
            targets=['esp32'], env_markers={'generic'}, runner_tags=('esp32', 'generic'),
            is_host_test=False, emulator_marker=None,
            apps=[SimpleNamespace(path='app', target='esp32', config='default')],
            item=SimpleNamespace(nodeid='app/pytest_app.py::test'),
        )
        eligible = {(str(Path('app').resolve()), 'esp32', 'default')}
        with patch.dict('sys.modules', {'idf_ci': SimpleNamespace(get_pytest_cases=lambda **kwargs: [case])}):
            allowed = GENERATOR.collect_test_groups(eligible)
            unavailable = GENERATOR.collect_test_groups(set())
        self.assertTrue(self.app(self.generate({'esp32': 3}, allowed), 'esp32')['run_tests'])
        matrices = self.generate({'esp32': 3}, unavailable)
        self.assertEqual(matrices['apps_matrix']['include'], [])
        self.assertEqual(len(matrices['extra_matrix']['include']), 1)

    def test_manifest_permissions_are_per_app_and_configuration(self):
        build = {'projects': {'app': {'apps': [
            {'target': 'esp32', 'config': '', 'build_status': 'should be built'},
            {'target': 'esp32', 'config': 'disabled', 'build_status': 'disabled'},
            {'target': 'esp32', 'config': 'compile-only', 'build_status': 'should be built',
             'matched_rules': {'disable_test': ['IDF_TARGET == "esp32"']}},
        ]}, 'other': {'apps': [
            {'target': 'esp32', 'config': 'default', 'build_status': 'disabled'},
        ]}}}
        with patch.object(GENERATOR, 'run_idf_ci', return_value=build), \
                patch.object(GENERATOR, 'collect_test_groups', return_value=[]) as collect:
            counts, _ = GENERATOR.collect_discovery()
        self.assertEqual(counts, {'esp32': 2})
        collect.assert_called_once_with({(str(Path('app').resolve()), 'esp32', 'default')})

    def test_manifest_exception_does_not_disable_unrelated_host_tests(self):
        build = {'projects': {
            'unity_app': {'apps': [{'target': 'linux', 'config': '', 'build_status': 'should be built',
                                   'matched_rules': {'disable_test': ['IDF_VERSION == "5.2"']}}]},
            'example': {'apps': [{'target': 'linux', 'config': '', 'build_status': 'should be built'}]},
        }}
        def case(path):
            return SimpleNamespace(
                targets=['linux'], env_markers={'host_test'}, runner_tags=('linux',),
                is_host_test=True, emulator_marker=None,
                apps=[SimpleNamespace(path=path, target='linux', config='default')],
                item=SimpleNamespace(nodeid=f'{path}/pytest_app.py::test[linux-idf]'),
            )
        cases = [case('unity_app'), case('example')]
        with patch.object(GENERATOR, 'run_idf_ci', return_value=build), \
                patch.dict('sys.modules', {'idf_ci': SimpleNamespace(get_pytest_cases=lambda **kwargs: cases)}):
            counts, groups = GENERATOR.collect_discovery()
        self.assertEqual(counts, {'linux': 2})
        app = self.app(self.generate(counts, groups), 'linux')
        self.assertTrue(app['run_tests'])
        self.assertEqual(app['tests'][0]['nodes'], ['example/pytest_app.py::test[linux-idf]'])


if __name__ == '__main__':
    unittest.main()
