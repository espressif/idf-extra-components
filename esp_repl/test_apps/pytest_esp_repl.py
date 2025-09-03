import pytest


@pytest.mark.generic
@pytest.mark.parametrize(
    'target', ['linux'], indirect=['target'])
def test_esp_repl(dut) -> None:
    dut.run_all_single_board_cases()
