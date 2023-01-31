/*
 * SPDX-FileCopyrightText: 2010-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
*/

#include <iostream>

#include <eigen3/Eigen/Eigen>

extern "C" void app_main(void);

static void multiply2Matrices()
{
    Eigen::MatrixXf M(2, 2);
    Eigen::MatrixXf V(2, 2);
    for (int i = 0; i <= 1; i++) {
        for (int j = 0; j <= 1; j++) {
            M(i, j) = 1;
            V(i, j) = 2;
        }
    }
    Eigen::MatrixXf Result = M * V;
    std::cout << "MatrixXf Result = " << std::endl << Result << std::endl;
}

static void runSVD()
{
    Eigen::MatrixXf C;
    C.setRandom(27, 18);
    Eigen::JacobiSVD<Eigen::MatrixXf> svd(C, Eigen::ComputeThinU | Eigen::ComputeThinV);
    Eigen::MatrixXf Cp = svd.matrixU() * svd.singularValues().asDiagonal() * svd.matrixV().transpose();
    Eigen::MatrixXf diff = Cp - C;
    std::cout << "SDV matrix U: " << std::endl << svd.matrixU() << std::endl;
    std::cout << "SDV singularValues: " << std::endl << svd.singularValues().transpose() << std::endl;
    std::cout << "SDV matrix V: " << std::endl << svd.matrixV() << std::endl;
    std::cout << "diff:\n" << diff.array().abs().sum() << "\n";
}

void app_main(void)
{
    std::cout << "Eigen example." << std::endl;
    multiply2Matrices();
    runSVD();
    std::cout << "Example finished!" << std::endl;
}
