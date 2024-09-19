import pytest


@pytest.mark.generic
def test_json_parser(dut) -> None:
    dut.run_all_single_board_cases()
