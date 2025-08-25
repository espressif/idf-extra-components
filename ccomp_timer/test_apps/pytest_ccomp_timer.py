import pytest


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32'], indirect=True)
def test_ccomp_timer(dut) -> None:
    dut.run_all_single_board_cases()
