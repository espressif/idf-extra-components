import pytest


@pytest.mark.parametrize(
    "marker",
    [
        pytest.param("qemu", marks=pytest.mark.qemu),
        pytest.param("generic", marks=pytest.mark.generic),
    ],
)
def test_esp_encrypted_img(dut, marker) -> None:
    dut.run_all_single_board_cases()
