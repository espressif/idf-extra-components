/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>
#include "Eigen/Core"
#include "Eigen/Geometry"
#include "Eigen/QR"
#include "Eigen/SVD"

extern "C" void app_main(void);

static void printSection(const char *title)
{
    std::cout << "\n===== " << title << " =====" << std::endl;
}

static void demoMatrixBasics()
{
    printSection("Matrix and Vector Basics");

    Eigen::Matrix2f A;
    // *INDENT-OFF*
    A << 1.0f, 2.0f,
         3.0f, 4.0f;
    // *INDENT-ON*
    Eigen::Vector2f v(5.0f, 6.0f);

    std::cout << "Given a 2x2 matrix A and a vector v:" << std::endl;
    std::cout << "A:\n" << A << std::endl;
    std::cout << "v = " << v.transpose() << std::endl;
    std::cout << "Matrix-vector multiplication combines each row of A with v:" << std::endl;
    std::cout << "A * v = " << (A * v).transpose() << std::endl;

    // For a 2x2 matrix, det(A) = a*d - b*c. If the determinant is non-zero,
    // the matrix is invertible and the transformation does not collapse the
    // plane into a line.
    std::cout << "det(A) = " << A.determinant()
              << " (non-zero means A is invertible)" << std::endl;

    // The Euclidean norm is the vector length: sqrt(v1^2 + v2^2).
    std::cout << "v.norm() = " << v.norm() << " (length of v)" << std::endl;
}

static void demoLinearSystem()
{
    printSection("Linear System");

    Eigen::Matrix3f A;
    // *INDENT-OFF*
    A << 3.0f, 1.0f, 0.0f,
         1.0f, 2.0f, 1.0f,
         0.0f, 1.0f, 3.0f;
    // *INDENT-ON*
    Eigen::Vector3f b(4.0f, 6.0f, 8.0f);

    // In linear algebra notation, we want to find the unknown vector x in
    //
    //                         A * x = b
    //
    // A is the coefficient matrix, b is the right-hand side, and x contains
    // the unknowns (x1, x2, x3). Printing the problem first makes it easier
    // to connect the code with the equations we are solving.
    std::cout << "Problem: solve A * x = b" << std::endl;
    std::cout << "A (coefficient matrix) =\n" << A << std::endl;
    std::cout << "b (right-hand side) = " << b.transpose() << std::endl;
    std::cout << "This represents the equations:" << std::endl;
    // Coefficients are written explicitly so the plus signs line up with
    // the columns of A.
    std::cout << "  3*x1 + 1*x2 + 0*x3 = 4" << std::endl;
    std::cout << "  1*x1 + 2*x2 + 1*x3 = 6" << std::endl;
    std::cout << "  0*x1 + 1*x2 + 3*x3 = 8" << std::endl;

    // QR decomposition is one way to solve this full-rank system without
    // explicitly computing A.inverse().
    Eigen::Vector3f x = A.colPivHouseholderQr().solve(b);

    std::cout << "Solution (unknown vector):" << std::endl;
    // Keep the compact form visible too: this is the value of x.
    std::cout << "solution x = " << x.transpose() << std::endl;
    std::cout << "Check: A * x = " << (A * x).transpose() << std::endl;

    // The residual is the part of b that is not reproduced by the solution:
    // r = A*x - b. For an exact solution, r should be zero (up to floating
    // point rounding). Its norm gives one number summarizing the error.
    Eigen::Vector3f residual = A * x - b;
    std::cout << "Residual r = A*x - b = " << residual.transpose() << std::endl;
    std::cout << "residual ||A*x - b|| = " << residual.norm() << std::endl;
}

static void demoSensorCalibration()
{
    printSection("Sensor Calibration (Least Squares)");

    // Four measured points from a sensor with a small amount of noise.
    // We want to estimate a slope 'a' and an offset 'b' in y = a*x + b.
    Eigen::Matrix<float, 4, 2> samples;
    // *INDENT-OFF*
    samples << 0.0f, 1.0f,
               1.0f, 1.0f,
               2.0f, 1.0f,
               3.0f, 1.0f;
    // *INDENT-ON*
    Eigen::Vector4f measured(1.1f, 2.9f, 5.2f, 6.8f);

    // Each row represents y = a*x + b. Since the measurements contain noise,
    // there may be no exact solution. QR finds the least-squares solution:
    // choose [a, b] to minimize ||samples * [a, b] - measured||.
    std::cout << "Problem: fit y = a*x + b to measured data" << std::endl;
    std::cout << "Each row of the design matrix is [x, 1]." << std::endl;
    std::cout << "design matrix X:\n" << samples << std::endl;
    std::cout << "measured y = " << measured.transpose() << std::endl;

    Eigen::Vector2f calibration = samples.colPivHouseholderQr().solve(measured);
    Eigen::Vector4f predicted = samples * calibration;
    Eigen::Vector4f fitResidual = predicted - measured;

    std::cout << "Solution (parameters [a, b]):" << std::endl;
    std::cout << "calibration: y = " << calibration(0) << " * x + "
              << calibration(1) << std::endl;
    std::cout << "predicted y = " << predicted.transpose() << std::endl;
    std::cout << "fit residual = predicted - measured = "
              << fitResidual.transpose() << std::endl;
    std::cout << "fit error ||predicted - measured|| = "
              << fitResidual.norm() << std::endl;

    // SVD provides another robust way to solve the same least-squares problem.
    // *INDENT-OFF*
    Eigen::JacobiSVD<Eigen::Matrix<float, 4, 2>,
                     Eigen::ComputeThinU | Eigen::ComputeThinV> svd(samples);
    // *INDENT-ON*
    Eigen::Vector2f svdCalibration = svd.solve(measured);
    std::cout << "SVD gives the same least-squares problem another way:"
              << std::endl;
    std::cout << "SVD calibration slope = " << svdCalibration(0) << std::endl;
}

static void demoGeometry()
{
    printSection("3D Geometry");

    using Eigen::AngleAxisf;
    using Eigen::Vector3f;

    const float angle = EIGEN_PI / 2.0f;
    const Vector3f axis = Vector3f::UnitZ();
    Eigen::Quaternionf rotation(AngleAxisf(angle, axis));
    Vector3f point(1.0f, 0.0f, 0.0f);
    Vector3f rotated = rotation * point;

    std::cout << "Problem: rotate a 3D point" << std::endl;
    std::cout << "rotation axis = " << axis.transpose()
              << ", angle = 90 degrees" << std::endl;
    std::cout << "A quaternion represents this 3D rotation without"
              << " using Euler angles." << std::endl;
    std::cout << "point = " << point.transpose() << std::endl;
    std::cout << "rotated point = " << rotated.transpose() << std::endl;
    std::cout << "point length = " << point.norm()
              << ", rotated length = " << rotated.norm()
              << " (rotation preserves length)" << std::endl;
}

static void demoMatrixSizes()
{
    printSection("Fixed and Dynamic Sizes");

    // Fixed-size matrices are a good choice for small, known-size problems.
    Eigen::Matrix3f fixed = Eigen::Matrix3f::Identity();

    // Dynamic-size matrices are useful when dimensions are known only at runtime.
    Eigen::MatrixXf dynamic(2, 2);
    // *INDENT-OFF*
    dynamic << 1.0f, 2.0f,
               3.0f, 4.0f;
    // *INDENT-ON*

    std::cout << "A fixed-size matrix has dimensions known at compile time:"
              << std::endl;
    std::cout << "fixed (3x3 identity matrix):\n" << fixed << std::endl;
    std::cout << "A dynamic-size matrix stores its dimensions at runtime:"
              << std::endl;
    std::cout << "dynamic (" << dynamic.rows() << "x" << dynamic.cols()
              << "):\n" << dynamic << std::endl;
    // The trace is the sum of the main diagonal, a simple matrix property.
    std::cout << "fixed 3x3 trace = " << fixed.trace() << std::endl;
    std::cout << "dynamic 2x2 trace = " << dynamic.trace() << std::endl;
}

void app_main(void)
{
    std::cout << "=== Eigen Example ===" << std::endl;

    demoMatrixBasics();
    demoLinearSystem();
    demoSensorCalibration();
    demoGeometry();
    demoMatrixSizes();

    std::cout << "\n=== Example finished! ===" << std::endl;
}
