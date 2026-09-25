"""Regression coverage for per-environment planning without ESP-IDF."""

import copy
import importlib.util
import unittest
from collections import Counter
from pathlib import Path
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / 'actions/ci-tools/generate_build_matrix.py'
SPEC = importlib.util.spec_from_file_location('generate_build_matrix', SCRIPT)
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)


def test_config(marker):
    return {'marker': marker, 'runner_labels': ['self-hosted', marker], 'pytest_args': ''}


def configuration():
    return {
        'idf_targets': ['esp32', 'esp32s3', 'linux'],
        'parallel_count': 3,
        'runs_per_job': 10,
        'test_configs': {
            'esp32': [test_config('generic'), test_config('ethernet'), test_config('spi_nand_flash')],
            'esp32s3': [test_config('qemu')],
            'linux': [test_config('host_test')],
        },
        'profiles': {
            'default': {},
            'no-linux-tests': {'exclude_markers': ['host_test']},
            'generic-only': {'exclude_markers': ['ethernet', 'spi_nand_flash', 'qemu', 'host_test']},
        },
    }


class GenerateMatricesTests(unittest.TestCase):
    def generate(self, counts=None, markers=None, profile='default', cfg=None):
        return GENERATOR.generate_matrices(
            configuration() if cfg is None else cfg, Counter(counts or {}), markers or {}, profile,
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
        self.assertEqual(self.app(second, 'esp32s3')['tests'], [test_config('qemu')])

    def test_intersects_discovery_with_available_runners(self):
        matrices = self.generate({'esp32': 1}, {'esp32': {'generic', 'unconfigured'}})
        self.assertEqual(self.app(matrices, 'esp32')['tests'], [test_config('generic')])

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
        self.assertEqual(self.app(matrices, 'esp32')['tests'], [test_config('generic')])
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

    def test_duplicate_markers_cannot_produce_colliding_artifacts(self):
        cfg = configuration()
        cfg['test_configs']['esp32'].append(test_config('generic'))
        with self.assertRaisesRegex(ValueError, 'Test markers must be unique'):
            self.generate({'esp32': 1}, {'esp32': {'generic'}}, cfg=cfg)

    def test_generation_does_not_mutate_inputs(self):
        cfg, counts, markers = configuration(), Counter(esp32=11), {'esp32': {'generic'}}
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
        generic = {'include': [{'nodes': 'test.py::test', 'targets': 'esp32', 'env_markers': 'generic'}]}
        qemu = {'include': [{'nodes': 'test.py::qemu', 'targets': 'esp32s3', 'env_markers': 'qemu'}]}
        host = {'include': [{'nodes': 'test.py::host', 'targets': 'linux', 'env_markers': ''}]}
        with patch.object(GENERATOR, 'run_idf_ci', side_effect=[build, generic, qemu, host]):
            counts, markers = GENERATOR.collect_discovery()
        self.assertEqual(counts, {'esp32': 1, 'linux': 1})
        self.assertEqual(markers, {'esp32': {'generic'}, 'esp32s3': {'qemu'}, 'linux': {'host_test'}})


if __name__ == '__main__':
    unittest.main()
