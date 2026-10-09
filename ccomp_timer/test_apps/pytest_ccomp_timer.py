import pytest


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32s2', 'esp32c3'], indirect=['target'])
def test_ccomp_timer(dut) -> None:
    dut.run_all_single_board_cases()
