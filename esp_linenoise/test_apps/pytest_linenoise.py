import pytest

@pytest.mark.generic
@pytest.mark.skip_if_soc("IDF_VERSION_MAJOR < 6")
def test_linenoise(dut) -> None:
    dut.run_all_single_board_cases()
