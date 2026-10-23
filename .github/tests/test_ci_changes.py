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

    def test_deleted_component_still_rebuilds_affected_dependents(self):
        result = self.select(['removed_component/idf_component.yml'])
        self.assertEqual(json.loads(result['modified_components']), ['removed_component'])
        self.assertEqual(result['has_changes'], 'true')


class WorkflowDiffSyntaxTests(unittest.TestCase):
    """The change list must come from a three-dot (merge-base) diff.

    A two-dot diff against the moving base branch lists commits the PR does
    not contain (un-rebased forks), which can flip the pipeline into a full
    run via CI-infra files. See PR #884 fallout.
    """

    WORKFLOW = Path(__file__).resolve().parents[1] / 'workflows' / 'build_and_run_apps.yml'

    def test_find_changes_diffs_against_merge_base(self):
        content = self.WORKFLOW.read_text()
        self.assertIn('git diff --name-only -r base_ref...pr_ref', content)
        self.assertNotIn('git diff --name-only -r base_ref pr_ref', content)


if __name__ == '__main__':
    unittest.main()
