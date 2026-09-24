import pytest


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32'], indirect=['target'])
def test_esp_delta_ota(dut) -> None:
    dut.run_all_single_board_cases()
