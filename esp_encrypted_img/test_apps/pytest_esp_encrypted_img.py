import pytest
import os


@pytest.mark.parametrize(
    "target, marker",
    [
        # Hardware and QEMU share a binary, but support different test environments.
        # An app-level disable_test for C3 would also disable its QEMU coverage.
        pytest.param("esp32", "generic", marks=pytest.mark.generic),
        pytest.param("esp32s2", "generic", marks=pytest.mark.generic),
        pytest.param("esp32s3", "generic", marks=pytest.mark.generic),
        pytest.param("esp32c3", "qemu", marks=pytest.mark.qemu),
        pytest.param("esp32s3", "qemu", marks=pytest.mark.qemu),
    ],
    indirect=["target"],
)
def test_esp_encrypted_img(dut, marker) -> None:
    binary_path = getattr(dut.app, "binary_path", None)
    if not binary_path or not os.path.exists(binary_path):
        pytest.skip(f"Build was skipped or binary not found: {binary_path}")
    dut.run_all_single_board_cases()
