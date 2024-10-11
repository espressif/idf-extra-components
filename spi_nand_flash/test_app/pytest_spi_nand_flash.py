import pytest


@pytest.mark.spi_nand_flash
def test_spi_nand_flash(dut) -> None:
    dut.run_all_single_board_cases()
