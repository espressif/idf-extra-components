import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@pytest.mark.skip_if_soc("IDF_VERSION_MAJOR < 6")
def test_esp_commands(dut) -> None:
    dut.run_all_single_board_cases()
