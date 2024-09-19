import pytest


@pytest.mark.generic
def test_expat(dut) -> None:
    dut.run_all_single_board_cases()
