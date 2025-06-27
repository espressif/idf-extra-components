import pytest

@pytest.mark.generic
def test_argtable3(dut) -> None:
    dut.run_all_single_board_cases()
