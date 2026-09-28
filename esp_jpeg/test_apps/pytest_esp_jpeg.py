import pytest


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32c2', 'esp32c3', 'esp32c5', 'esp32c6', 'esp32c61', 'esp32h2', 'esp32p4', 'esp32s2', 'esp32s3'], indirect=['target'])
def test_esp_jpeg(dut) -> None:
    dut.run_all_single_board_cases()
