import pytest

@pytest.mark.generic
@pytest.mark.parametrize(
    'target', ['linux'], indirect=['target'])
@pytest.mark.skip_if_soc("IDF_VERSION_MAJOR < 5 and IDF_VERSION_MINOR <= 4")
def test_esp_linenoise(dut) -> None:
    dut.run_all_single_board_cases()
