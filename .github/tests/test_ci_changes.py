"""Version and test-policy changes must exercise the version pipelines."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'get_ci_changes.py'


class ChangeSelectionTests(unittest.TestCase):
    def select(self, files):
        with tempfile.TemporaryDirectory() as root:
            Path(root, 'component').mkdir()
            changed = Path(root, 'changed.txt')
            changed.write_text('\n'.join(files))
            output = Path(root, 'outputs')
            subprocess.run([sys.executable, str(SCRIPT), str(changed)], cwd=root,
                           env=dict(os.environ, GITHUB_OUTPUT=str(output)), check=True)
            return dict(line.split('=', 1) for line in output.read_text().splitlines())

    def test_adding_version_runs_full_inventory(self):
        result = self.select(['.github/ci-matrix.json'])
        self.assertEqual(result, {'has_changes': 'true', 'modified_files': '[]', 'modified_components': '[]'})

    def test_global_policy_changes_clear_component_filter(self):
        for path in ['conftest.py', 'pytest.ini', '.idf_build_apps.toml', '.build-test-rules.yml',
                     '.github/actions/ci-tools/generate_build_matrix.py',
                     '.github/workflows/reusable-ci.yml']:
            with self.subTest(path=path):
                result = self.select([path, 'component/pytest_example.py'])
                self.assertEqual(result['modified_components'], '[]')
                self.assertEqual(result['modified_files'], '[]')
                self.assertEqual(result['has_changes'], 'true')

    def test_component_test_change_stays_scoped(self):
        result = self.select(['component/pytest_example.py'])
        self.assertEqual(json.loads(result['modified_components']), ['component'])
        self.assertEqual(result['has_changes'], 'true')

    def test_documentation_only_change_does_not_start_builds(self):
        self.assertEqual(self.select(['README.md', '.github/readme_workflows.md'])['has_changes'], 'false')


if __name__ == '__main__':
    unittest.main()
