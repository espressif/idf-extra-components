import pytest


@pytest.mark.generic
def test_ccomp_timer(dut) -> None:
    dut.run_all_single_board_cases()
