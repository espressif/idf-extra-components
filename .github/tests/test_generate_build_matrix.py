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


def configuration():
    import json
    cfg = json.loads((SCRIPT.parents[2] / 'ci-config.json').read_text())
    cfg['idf_targets'] = ['esp32', 'esp32s3', 'linux']
    cfg['parallel_count'] = 3
    cfg['runs_per_job'] = 10
    cfg['profiles']['generic-only'] = {'exclude_markers': ['ethernet', 'spi_nand_flash', 'qemu', 'host_test']}
    return cfg


class GenerateMatricesTests(unittest.TestCase):
    def generate(self, counts=None, markers=None, profile='default', cfg=None):
        return GENERATOR.generate_matrices(
            configuration() if cfg is None else cfg, Counter(counts or {}), ([group(target, [marker]) for target, markers_for_target in markers.items() for marker in markers_for_target] if isinstance(markers, dict) else markers or []), profile,
        )

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

    def test_profile_excludes_whole_combined_group_but_preserves_qemu(self):
        cfg = configuration()
        cfg['profiles']['without-psram'] = {'exclude_markers': ['quad_psram']}
        tests = self.app(self.generate({'esp32': 1}, [
            group('esp32', ['generic']), group('esp32', ['generic', 'quad_psram']),
        ], 'without-psram', cfg), 'esp32')['tests']
        self.assertEqual([test['marker'] for test in tests], ['generic'])
        matrices = self.generate({'esp32s3': 1, 'linux': 1}, [
            group('esp32s3', ['qemu']), group('linux', ['host_test']),
        ], 'no-linux-tests')
        self.assertTrue(self.app(matrices, 'esp32s3')['run_tests'])
        self.assertFalse(self.app(matrices, 'linux')['run_tests'])

    def test_multi_dut_is_rejected_instead_of_split_into_invalid_jobs(self):
        multi = group('esp32', ['generic'])
        multi['targets'] = ['esp32', 'esp32']
        with self.assertRaisesRegex(ValueError, 'Multi-DUT'):
            self.generate({'esp32': 1}, [multi])

    def test_profile_disables_linux_tests_but_keeps_linux_build(self):
        counts = {'esp32': 1, 'linux': 11}
        markers = {'esp32': {'generic'}, 'linux': {'host_test'}}
        full = self.generate(counts, markers)
        reduced = self.generate(counts, markers, 'no-linux-tests')
        self.assertTrue(self.app(full, 'linux')['run_tests'])
        self.assertFalse(self.app(reduced, 'linux')['run_tests'])
        self.assertEqual(self.app(reduced, 'linux')['parallel_count'], 2)
        self.assertTrue(self.app(reduced, 'esp32')['run_tests'])
        self.assertEqual(reduced['extra_matrix']['include'], [])

    def test_profile_treats_runner_markers_uniformly(self):
        matrices = self.generate(
            {'esp32': 1, 'esp32s3': 1},
            {'esp32': {'generic', 'ethernet', 'spi_nand_flash'}, 'esp32s3': {'qemu'}},
            'generic-only',
        )
        self.assertEqual([t['marker'] for t in self.app(matrices, 'esp32')['tests']], ['generic'])
        self.assertFalse(self.app(matrices, 'esp32s3')['run_tests'])

    def test_empty_discovery_has_no_fallback_jobs(self):
        matrices = self.generate({}, {'esp32': {'generic'}})
        self.assertEqual(matrices, {'apps_matrix': {'include': []}, 'extra_matrix': {'include': []}})

    def test_shards_round_up_and_obey_cap(self):
        for count, expected in [(1, 1), (10, 1), (11, 2), (20, 2), (21, 3), (100, 3)]:
            with self.subTest(count=count):
                matrices = self.generate({'esp32': count, 'esp32c6': count})
                app = self.app(matrices, 'esp32')
                self.assertEqual(app['parallel_count'], expected)
                self.assertEqual(app['parallel_indices'], list(range(1, expected + 1)))
                self.assertEqual(matrices['extra_matrix']['include'], [
                    {'parallel_index': i, 'parallel_count': expected} for i in range(1, expected + 1)
                ])

    def test_extra_counts_only_targets_outside_the_target_workers(self):
        matrices = self.generate({'esp32': 100, 'linux': 100, 'esp32c6': 6, 'esp32h2': 5})
        self.assertEqual(len(matrices['extra_matrix']['include']), 2)
        self.assertTrue(all(e['parallel_count'] == 2 for e in matrices['extra_matrix']['include']))

    def test_no_cases_keeps_build_but_disables_tests(self):
        app = self.app(self.generate({'esp32': 1}), 'esp32')
        self.assertFalse(app['run_tests'])
        self.assertEqual(app['tests'], [])

    def test_invalid_policy_fails_early(self):
        for key in ['parallel_count', 'runs_per_job']:
            cfg = configuration()
            cfg[key] = 0
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.generate(cfg=cfg)
        with self.assertRaisesRegex(ValueError, 'Unknown test profile'):
            self.generate(profile='typo')

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
        cfg, counts, markers = configuration(), Counter(esp32=11), [group('esp32', ['generic'])]
        before = copy.deepcopy((cfg, counts, markers))
        first = GENERATOR.generate_matrices(cfg, counts, markers)
        second = GENERATOR.generate_matrices(cfg, counts, markers)
        self.assertEqual((cfg, counts, markers), before)
        self.assertEqual(first, second)

    def test_collection_counts_only_buildable_apps_and_keeps_emulators_and_host(self):
        build = {'projects': {'app': {'apps': [
            {'target': 'esp32', 'build_status': 'should be built'},
            {'target': 'esp32', 'build_status': 'disabled'},
            {'target': 'linux', 'build_status': 'should be built'},
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
                                   item=SimpleNamespace(nodeid=node))

        linux = case('linux', [], 'test.py::host[linux]', host=True)
        runtime_linux = case('linux', [], 'test.py::host[linux-idf]', host=True)
        qemu = case('esp32s3', ['qemu'], 'test.py::emulator[esp32s3-qemu]', host=True, emulator='qemu')
        psram = case('esp32s3', ['generic', 'quad_psram'], 'test.py::hardware[with spaces]')
        collect = Mock(side_effect=[[linux, qemu, psram], [runtime_linux]])
        with patch.dict('sys.modules', {'idf_ci': SimpleNamespace(get_pytest_cases=collect)}):
            groups = GENERATOR.collect_test_groups()
        self.assertEqual(collect.call_args_list[1].kwargs['target'], 'linux')
        self.assertEqual(next(g['nodes'] for g in groups if g['targets'] == ['linux']), [runtime_linux.item.nodeid])
        self.assertEqual(next(g['markers'] for g in groups if 'qemu' in g['markers']), ['qemu'])
        self.assertEqual(next(g['markers'] for g in groups if 'quad_psram' in g['markers']), ['generic', 'quad_psram'])



if __name__ == '__main__':
    unittest.main()
