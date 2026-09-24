import pytest
import os


@pytest.mark.parametrize(
    "marker",
    [
        pytest.param("qemu", marks=pytest.mark.qemu),
        pytest.param("generic", marks=pytest.mark.generic),
    ],
)
@pytest.mark.parametrize('target', ['esp32', 'esp32s2', 'esp32s3', 'esp32c3'], indirect=['target'])
def test_esp_encrypted_img(dut, marker) -> None:
    binary_path = getattr(dut.app, "binary_path", None)
    if not binary_path or not os.path.exists(binary_path):
        pytest.skip(f"Build was skipped or binary not found: {binary_path}")
    dut.run_all_single_board_cases()
