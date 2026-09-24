import pytest


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32s2', 'esp32s3', 'esp32c3'], indirect=['target'])
def test_expat(dut) -> None:
    dut.run_all_single_board_cases()
