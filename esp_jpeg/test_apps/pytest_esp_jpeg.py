import pytest


@pytest.mark.disable_test(
    'IDF_TARGET not in ["esp32", "esp32s2", "esp32s3"]',
    reason='Preserve the previous CI runtime coverage; enable additional hardware targets after runner validation',
)
@pytest.mark.disable_test(
    'IDF_TARGET == "esp32s3"',
    reason='Newly selectable ESP32-S3 tests hit serial write timeouts (CI run 38376778030); retain builds pending runner/board investigation',
)
@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32s2', 'esp32s3'], indirect=['target'])
def test_esp_jpeg(dut) -> None:
    dut.run_all_single_board_cases()
