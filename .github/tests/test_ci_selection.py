"""Exact discovery groups must not expand to other runners' test cases."""

import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import Mock

SCRIPT = Path(__file__).resolve().parents[1] / 'actions/ci-tools/run_tests.py'
SPEC = importlib.util.spec_from_file_location('run_tests', SCRIPT)
RUN_TESTS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUN_TESTS)


class SelectionTests(unittest.TestCase):
    def test_selects_exact_nodes_including_parameters_with_spaces(self):
        plain = SimpleNamespace(nodeid='app/pytest_app.py::test_generic[esp32]')
        special = SimpleNamespace(nodeid='app/pytest_app.py::test_generic_psram[with spaces]')
        other = SimpleNamespace(nodeid='app/pytest_app.py::test_generic[esp32s3]')
        items = [plain, special, other]
        config = SimpleNamespace(hook=SimpleNamespace(pytest_deselected=Mock()))
        RUN_TESTS.SelectedNodes([special.nodeid]).pytest_collection_modifyitems(config, items)
        self.assertEqual(items, [special])
        config.hook.pytest_deselected.assert_called_once_with(items=[plain, other])

    def test_nodes_removed_by_build_metadata_are_not_reintroduced(self):
        selection = RUN_TESTS.SelectedNodes(['skipped-app/pytest_app.py::test'])
        items = [SimpleNamespace(nodeid='built-app/pytest_app.py::test')]
        config = SimpleNamespace(hook=SimpleNamespace(pytest_deselected=Mock()))
        selection.pytest_collection_modifyitems(config, items)
        self.assertEqual(items, [])

    def test_empty_group_selects_nothing_and_invalid_json_shape_fails(self):
        config = SimpleNamespace(hook=SimpleNamespace(pytest_deselected=Mock()))
        items = [SimpleNamespace(nodeid='app.py::test')]
        RUN_TESTS.SelectedNodes([]).pytest_collection_modifyitems(config, items)
        self.assertEqual(items, [])
        for value in ['app.py::test', None, [1]]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                RUN_TESTS.SelectedNodes(value)


if __name__ == '__main__':
    unittest.main()
