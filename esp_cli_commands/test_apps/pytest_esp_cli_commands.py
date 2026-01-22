import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize
import glob
from pathlib import Path


@pytest.mark.host_test
@pytest.mark.skipif(
    not bool(glob.glob(f'{Path(__file__).parent.absolute()}/build*/')),
    reason="Skip the idf version that did not build"
)
@idf_parametrize('target', ['linux', 'esp32'], indirect=['target'])
def test_esp_cli_commands(dut) -> None:
    dut.run_all_single_board_cases()
