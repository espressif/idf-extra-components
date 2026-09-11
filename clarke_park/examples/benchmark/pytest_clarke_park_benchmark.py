import pytest


@pytest.mark.generic
def test_clarke_park_benchmark(dut) -> None:
    dut.expect_exact('clarke_park benchmark finished', timeout=60)
