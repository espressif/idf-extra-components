import pytest


@pytest.mark.disable_test(
    'IDF_TARGET == "esp32c3"',
    reason='Preserve the previous CI runtime coverage; enable additional hardware targets after runner validation',
)
@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32s2', 'esp32c3'], indirect=['target'])
def test_ccomp_timer(dut) -> None:
    dut.run_all_single_board_cases()
