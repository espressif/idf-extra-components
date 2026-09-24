import pytest


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32c3'], indirect=['target'])
def test_coremark(dut):
    dut.expect_exact("Running coremark...")
    dut.expect_exact("Correct operation validated", timeout=30)
