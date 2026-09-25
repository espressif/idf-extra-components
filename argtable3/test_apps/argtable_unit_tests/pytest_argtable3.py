import pytest
from pytest_embedded import Dut


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32c3'], indirect=['target'])
def test_argtable3(dut: Dut) -> None:
    dut.run_all_single_board_cases()
