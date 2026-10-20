"""Discovery exclusions must preserve sibling targets/configurations."""

import importlib.util
import os
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch


def mark(**kwargs):
    return SimpleNamespace(args=(), kwargs=kwargs)


def item(target='esp32', config='default', rules=(), apps=None):
    apps = apps or [SimpleNamespace(target=target, config=config)]
    return SimpleNamespace(
        nodeid=f'test.py::test[{target}-{config}]',
        iter_markers=lambda name: rules,
        case=SimpleNamespace(apps=apps, targets=[app.target for app in apps]),
    )


class DiscoveryMarkerTests(unittest.TestCase):
    def setUp(self):
        environment = patch.dict(os.environ, {'TEST_NODES': ''})
        environment.start()
        self.addCleanup(environment.stop)
        spec = importlib.util.spec_from_file_location('ci_conftest', Path(__file__).resolve().parents[2] / 'conftest.py')
        self.hooks = importlib.util.module_from_spec(spec)
        self.modules = {
            'pytest': SimpleNamespace(fixture=lambda f: f, hookimpl=lambda **kw: lambda f: f, UsageError=ValueError),
            '_pytest.fixtures': SimpleNamespace(FixtureRequest=object),
            'pytest_embedded.plugin': SimpleNamespace(multi_dut_fixture=lambda f: f),
            'idf_ci.idf_pytest.plugin': SimpleNamespace(
                IdfPytestPlugin=SimpleNamespace(get_case_by_item=lambda test: test.case)),
        }
        with patch.dict('sys.modules', self.modules):
            spec.loader.exec_module(self.hooks)

    def collect(self, items):
        hook = Mock()
        config = SimpleNamespace(hook=SimpleNamespace(pytest_deselected=hook))
        with patch.dict('sys.modules', self.modules):
            self.hooks.pytest_collection_modifyitems(config, items)
        return items, hook

    def test_target_and_config_must_match_the_same_variant(self):
        rule = mark(targets=['esp32c3'], configs=['rmt'], reason='No runner')
        cases = [item(target, config, [rule]) for target in ['esp32', 'esp32c3'] for config in ['rmt', 'uart']]
        excluded = cases[2]
        remaining, hook = self.collect(cases)
        self.assertEqual([c.nodeid for c in remaining], [
            'test.py::test[esp32-rmt]', 'test.py::test[esp32-uart]', 'test.py::test[esp32c3-uart]'])
        hook.assert_called_once_with(items=[excluded])

    def test_omitted_selectors_and_multiple_rules(self):
        rules = [mark(targets=['esp32c3'], reason='Target disabled'), mark(configs=['rmt'], reason='Config disabled')]
        remaining, _ = self.collect([item('esp32', 'uart', rules), item('esp32', 'rmt', rules),
                                     item('esp32c3', 'uart', rules), item(rules=[mark(reason='Whole test disabled')])])
        self.assertEqual([c.nodeid for c in remaining], ['test.py::test[esp32-uart]'])

    def test_multi_app_rule_does_not_mix_target_and_config_from_different_apps(self):
        rule = mark(targets=['esp32c3'], configs=['rmt'], reason='Unsupported pair')
        mixed = item(rules=[rule], apps=[SimpleNamespace(target='esp32', config='rmt'),
                                       SimpleNamespace(target='esp32c3', config='uart')])
        self.assertEqual(self.collect([mixed])[0], [mixed])

    def test_unmarked_cases_are_unchanged(self):
        original = [item(), item('linux')]
        selected, hook = self.collect(original.copy())
        self.assertEqual(selected, original)
        hook.assert_not_called()

    def test_invalid_rules_fail_discovery(self):
        for rule in [mark(), mark(reason=' '), mark(target='esp32', reason='Typo'),
                     mark(targets='esp32', reason='Not a list'), mark(configs=[], reason='Empty'),
                     mark(targets=[1], reason='Wrong type'),
                     SimpleNamespace(args=('a', 'b'), kwargs={'reason': 'Too many conditions'}),
                     SimpleNamespace(args=('',), kwargs={'reason': 'Empty expression'}),
                     SimpleNamespace(args=(False,), kwargs={'reason': 'Not an IDF expression'})]:
            with self.subTest(rule=rule), self.assertRaisesRegex(ValueError, 'disable_test'):
                self.collect([item(rules=[rule])])

    def test_expression_uses_esp_bool_parser_and_is_parsed_once(self):
        expression = 'IDF_VERSION < "5.5" and CONFIG_NAME == "rmt"'
        evaluate = Mock(side_effect=lambda target, config: config == 'rmt')
        parse = Mock(return_value=SimpleNamespace(get_value=evaluate))
        self.modules.update({
            'esp_bool_parser': SimpleNamespace(parse_bool_expr=parse),
            'esp_bool_parser.utils': SimpleNamespace(InvalidIfClause=SystemExit),
            'pyparsing': SimpleNamespace(ParseBaseException=ValueError),
        })
        rule = SimpleNamespace(args=(expression,), kwargs={'targets': ['esp32c3'], 'reason': 'SDK issue'})
        cases = [item(target, config, [rule]) for target in ['esp32', 'esp32c3'] for config in ['rmt', 'uart']]
        remaining, _ = self.collect(cases)
        self.assertEqual(len(remaining), 3)
        self.assertNotIn('test.py::test[esp32c3-rmt]', [case.nodeid for case in remaining])
        parse.assert_called_once_with(expression)
        self.assertEqual(evaluate.call_args_list, [(('esp32c3', 'rmt'),), (('esp32c3', 'uart'),)])

    def test_invalid_expression_reports_node_and_does_not_silently_disable(self):
        self.modules.update({
            'esp_bool_parser': SimpleNamespace(parse_bool_expr=Mock(side_effect=ValueError('Invalid if clause'))),
            'esp_bool_parser.utils': SimpleNamespace(InvalidIfClause=SystemExit),
            'pyparsing': SimpleNamespace(ParseBaseException=ValueError),
        })
        rule = SimpleNamespace(args=('broken expression',), kwargs={'reason': 'Invalid'})
        with self.assertRaisesRegex(ValueError, 'test.py::test.*disable_test.*Invalid if clause'):
            self.collect([item(rules=[rule])])

    def test_ci_worker_keeps_frozen_nodes_without_evaluating_sdk_rules(self):
        rule = SimpleNamespace(args=('IDF_VERSION < "5.5"',), kwargs={'reason': 'SDK issue'})
        original = [item(rules=[rule]), item(rules=[mark(reason='Already evaluated')])]
        with patch.dict(os.environ, {'TEST_NODES': '["test.py::test[esp32-default]"]'}):
            remaining, hook = self.collect(original.copy())
        self.assertEqual(remaining, original)
        hook.assert_not_called()

    def test_audit_retains_disabled_nodes_with_native_report_skip_reasons(self):
        node = item(rules=[mark(reason='Runner unavailable'), mark(reason='Under investigation')])
        node.add_marker = Mock()
        items = [node]
        native_marker = Mock(return_value='native-skip-marker')
        self.hooks.pytest.mark = SimpleNamespace(temp_skip=native_marker)
        config = SimpleNamespace(getoption=lambda name: True, addinivalue_line=Mock(),
                                 hook=SimpleNamespace(pytest_deselected=Mock()))
        with patch.dict(os.environ, {'IDF_CI_INCLUDE_DISABLED_TESTS': '1'}), \
                patch.dict('sys.modules', self.modules):
            self.hooks.pytest_collection_modifyitems(config, items)
        self.assertEqual(items, [node])
        native_marker.assert_called_once_with(targets=['esp32'], reason='Runner unavailable; Under investigation')
        node.add_marker.assert_called_once_with('native-skip-marker')
        config.hook.pytest_deselected.assert_not_called()


if __name__ == '__main__':
    unittest.main()
