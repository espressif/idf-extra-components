import pytest


@pytest.mark.generic
def test_iqmath_example(dut) -> None:
    dut.expect_exact("IQMath test passed")
