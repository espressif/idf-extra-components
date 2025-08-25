import pytest
from pytest_embedded import Dut


@pytest.mark.generic
@pytest.mark.skip_if_soc("IDF_VERSION_MAJOR < 6")
@pytest.mark.parametrize('target', ['esp32'], indirect=True)
def test_argtable3(dut) -> None:
    dut.run_all_single_board_cases()
