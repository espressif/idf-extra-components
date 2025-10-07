import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@pytest.mark.parametrize(
    'target', ['linux'], indirect=['target'])
def test_esp_commands(dut) -> None:
    dut.run_all_single_board_cases()
