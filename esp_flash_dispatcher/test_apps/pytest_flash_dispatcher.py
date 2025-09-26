import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@pytest.mark.skip_if_soc("SOC_SPIRAM_XIP_SUPPORTED != 1")
def test_esp_flash_dispatcher(dut) -> None:
    dut.run_all_single_board_cases()
