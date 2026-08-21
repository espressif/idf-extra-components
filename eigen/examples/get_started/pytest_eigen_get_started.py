# SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import pytest
from pytest_embedded import Dut


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32s3', 'esp32c3'], indirect=['target'])
def test_eigen_get_started(dut: Dut) -> None:
    """Test the Eigen get-started example output."""
    dut.expect_exact('=== Eigen Example ===')
    dut.expect_exact('===== Matrix and Vector Basics =====')
    dut.expect_exact('Given a 2x2 matrix A and a vector v:')
    dut.expect_exact('A * v = ')
    dut.expect_exact('det(A) = ')
    dut.expect_exact('v.norm() = ')

    dut.expect_exact('===== Linear System =====')
    dut.expect_exact('Problem: solve A * x = b')
    dut.expect_exact('A (coefficient matrix) =')
    dut.expect_exact('This represents the equations:')
    dut.expect_exact('solution x = ')
    dut.expect_exact('Check: A * x = ')
    dut.expect_exact('Residual r = A*x - b = ')
    dut.expect_exact('residual ||A*x - b|| = ')

    dut.expect_exact('===== Sensor Calibration (Least Squares) =====')
    dut.expect_exact('Problem: fit y = a*x + b to measured data')
    dut.expect_exact('design matrix X:')
    dut.expect_exact('calibration: y = ')
    dut.expect_exact('predicted y = ')
    dut.expect_exact('fit residual = predicted - measured = ')
    dut.expect_exact('fit error ||predicted - measured|| = ')
    dut.expect_exact('SVD calibration slope = ')

    dut.expect_exact('===== 3D Geometry =====')
    dut.expect_exact('Problem: rotate a 3D point')
    dut.expect_exact('rotation axis = ')
    dut.expect_exact('rotated point = ')
    dut.expect_exact('rotation preserves length')

    dut.expect_exact('===== Fixed and Dynamic Sizes =====')
    dut.expect_exact('fixed (3x3 identity matrix):')
    dut.expect_exact('dynamic (2x2):')
    dut.expect_exact('fixed 3x3 trace = ')
    dut.expect_exact('dynamic 2x2 trace = ')

    dut.expect_exact('=== Example finished! ===')
