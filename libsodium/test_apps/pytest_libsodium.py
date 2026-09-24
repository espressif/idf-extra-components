import pytest


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32c3'], indirect=['target'])
def test_libsodium(dut) -> None:
    dut.run_all_single_board_cases(timeout=120)
