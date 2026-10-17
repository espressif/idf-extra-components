"""Runtime collection must execute the exact post-build plan or fail."""

import importlib.util
import os
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

SCRIPT = Path(__file__).resolve().parents[1] / 'actions/ci-tools/run_tests.py'
SPEC = importlib.util.spec_from_file_location('run_tests', SCRIPT)
RUN_TESTS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUN_TESTS)


class SelectionTests(unittest.TestCase):
    def check(self, planned, collected):
        session = SimpleNamespace(items=[SimpleNamespace(nodeid=node) for node in collected])
        with patch.dict('sys.modules', {'pytest': SimpleNamespace(UsageError=ValueError)}):
            RUN_TESTS.SelectedNodes(planned).pytest_collection_finish(session)

    def test_exact_nodes_including_parameters_with_spaces(self):
        nodes = ['app/pytest_app.py::test[with spaces]', 'app/pytest_app.py::test[$(not-a-command)]']
        self.check(nodes, list(reversed(nodes)))

    def test_empty_or_partial_runtime_collection_fails(self):
        for actual in [[], ['app.py::test_a']]:
            with self.subTest(actual=actual), self.assertRaisesRegex(ValueError, 'missing=.*test_b'):
                self.check(['app.py::test_a', 'app.py::test_b'], actual)

    def test_unplanned_nodes_fail(self):
        with self.assertRaisesRegex(ValueError, 'unexpected=.*other'):
            self.check(['app.py::test'], ['app.py::test', 'other.py::test'])

    def test_empty_or_invalid_plan_is_rejected(self):
        for value in [[], '', None, [1], ['']]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                RUN_TESTS.SelectedNodes(value)

    def test_cli_passes_node_ids_literally_and_preserves_pytest_failure(self):
        node = 'app/pytest_app.py::test[with spaces;$(not-a-command)]'
        import json
        run = Mock(return_value=1)
        with patch.dict('sys.modules', {'pytest': SimpleNamespace(main=run)}), \
                patch.dict(os.environ, {'TEST_NODES': json.dumps([node]), 'PYTEST_ARGS': '--embedded-services idf,qemu'}), \
                patch('sys.argv', ['run_tests', '--target', 'esp32c3', '--junit-xml', 'results.xml']):
            with self.assertRaises(SystemExit) as error:
                RUN_TESTS.main()
        self.assertEqual(error.exception.code, 1)
        command = run.call_args.args[0]
        self.assertEqual(command[0], node)
        self.assertNotIn('--suppress-no-test-exit-code', command)
        self.assertEqual(command[-2:], ['--embedded-services', 'idf,qemu'])


if __name__ == '__main__':
    unittest.main()
