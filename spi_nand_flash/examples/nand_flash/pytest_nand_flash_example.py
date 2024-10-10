import pytest


@pytest.mark.spi_nand_flash
def test_nand_flash_example(dut) -> None:
    dut.expect_exact("Opening file")
    dut.expect_exact("File written")
    dut.expect_exact("Reading file")
    dut.expect_exact("Read from file:")
    dut.expect_exact("Returned from app_main")
