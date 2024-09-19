import pytest


@pytest.mark.generic
def test_coremark(dut):
    dut.expect_exact("Running coremark...")
    dut.expect_exact("Correct operation validated", timeout=30)
