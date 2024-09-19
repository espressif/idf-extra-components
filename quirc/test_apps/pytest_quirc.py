import pytest


@pytest.mark.generic
def test_quirc(dut) -> None:
    dut.run_all_single_board_cases()
