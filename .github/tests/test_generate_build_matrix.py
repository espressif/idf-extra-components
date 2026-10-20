"""Build planning includes apps with and without runtime tests."""

import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

SCRIPT = Path(__file__).resolve().parents[1] / 'actions/ci-tools/generate_build_matrix.py'
SPEC = importlib.util.spec_from_file_location('generate_build_matrix', SCRIPT)
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)


class BuildMatrixTests(unittest.TestCase):
    def test_discovery_counts_test_and_build_only_apps_but_not_skipped_variants(self):
        collect = Mock(return_value=(
            [SimpleNamespace(target='esp32', build_status='build'),
             SimpleNamespace(target='linux', build_status='build')],
            [SimpleNamespace(target='new_supported_chip', build_status='build'),
             SimpleNamespace(target='linux', build_status='disabled'),
             SimpleNamespace(target='esp32', build_status='skipped')],
        ))
        find = Mock(return_value=[])
        modules = {
            'idf_ci': SimpleNamespace(get_all_apps=collect),
            'idf_build_apps': SimpleNamespace(find_apps=find),
            'idf_build_apps.constants': SimpleNamespace(
                BuildStatus=SimpleNamespace(SHOULD_BE_BUILT='build'), SUPPORTED_TARGETS=['esp32']),
        }
        with patch.dict('sys.modules', modules):
            self.assertEqual(GENERATOR.collect_app_counts(), {'esp32': 1, 'new_supported_chip': 1, 'linux': 1})
        collect.assert_called_once_with(marker_expr='')
        find.assert_called_once_with(['.'], 'linux', default_build_targets=['esp32'])

    def test_versions_have_independent_targets_and_shards(self):
        first = GENERATOR.generate_build_matrix({'esp32': 21})['include']
        second = GENERATOR.generate_build_matrix({'esp32s3': 1})['include']
        self.assertEqual(first, [
            {'idf_target': 'esp32', 'parallel_index': 1, 'parallel_count': 2},
            {'idf_target': 'esp32', 'parallel_index': 2, 'parallel_count': 2},
        ])
        self.assertEqual(second, [{'idf_target': 'esp32s3', 'parallel_index': 1, 'parallel_count': 1}])

    def test_all_targets_get_builds_without_needing_pytest_cases(self):
        jobs = GENERATOR.generate_build_matrix({'linux': 1, 'esp32h4': 1, 'esp32': 1})['include']
        self.assertEqual([job['idf_target'] for job in jobs], ['esp32', 'esp32h4', 'linux'])
        self.assertTrue(all('tests' not in job for job in jobs))

    def test_shards_round_up_at_twenty_without_the_old_five_shard_cap(self):
        for count, expected in [(0, 0), (1, 1), (20, 1), (21, 2), (100, 5), (101, 6)]:
            with self.subTest(count=count):
                jobs = GENERATOR.generate_build_matrix({'esp32': count})['include']
                self.assertEqual(len(jobs), expected)
                self.assertEqual([j['parallel_index'] for j in jobs], list(range(1, expected + 1)))
                self.assertTrue(all(j['parallel_count'] == expected for j in jobs))

    def test_empty_discovery_has_no_fallback_job(self):
        self.assertEqual(GENERATOR.generate_build_matrix({}), {'include': []})


if __name__ == '__main__':
    unittest.main()
