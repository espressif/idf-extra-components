import pytest


@pytest.mark.spi_nand_flash
def test_nand_flash_debug_example(dut) -> None:
    dut.expect_exact("Get bad block statistics:")
    dut.expect_exact("Read-Write Throughput via Dhara:")
    dut.expect_exact("Read-Write Throughput at lower level (bypassing Dhara):")
    dut.expect_exact("ECC errors statistics:")
    dut.expect_exact("Returned from app_main")
