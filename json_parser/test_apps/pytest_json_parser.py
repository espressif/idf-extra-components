import pytest
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32'], indirect=['target'])
def test_json_parser(dut) -> None:
    dut.run_all_single_board_cases()
