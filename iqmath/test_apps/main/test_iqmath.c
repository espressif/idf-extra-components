/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include "unity.h"
#include "esp_log.h"
#include "IQmathLib.h"

#define ERROR_WITHIN_TOLERANCE(result, expected, tolerance) \
    (((result) >= ((expected) - ((expected) * (tolerance)))) && \
    ((result) <= ((expected) + ((expected) * (tolerance)))))

static const char *TAG = "iqmath_test";

TEST_CASE("Test IQmath basic arithmetic", "[iqmath]")
{
    const float error_tolerance = 0.01;
    float res;

    /* IQ variables using global type */
    _iq qA = _IQ(1.5);
    _iq qB = _IQ(2.5);
    _iq qC;

    qC = qA + qB;
    res = _IQtoF(qC);
    ESP_LOGI(TAG, "Addition: %f + %f = %f", _IQtoF(qA), _IQtoF(qB), res);
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(res, 4.0, error_tolerance));

    qC = _IQmpy(qA, qB);
    res = _IQtoF(qC);
    ESP_LOGI(TAG, "Multiplication: %f * %f = %f", _IQtoF(qA), _IQtoF(qB), res);
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(res, 3.75, error_tolerance));

    qC = qB - qA;
    res = _IQtoF(qC);
    ESP_LOGI(TAG, "Subtraction: %f - %f = %f", _IQtoF(qB), _IQtoF(qA), res);
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(res, 1.0, error_tolerance));

    qC = _IQdiv(qB, qA);
    res = _IQtoF(qC);
    ESP_LOGI(TAG, "Division: %f / %f = %f", _IQtoF(qB), _IQtoF(qA), res);
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(res, 1.666667, error_tolerance));
}

TEST_CASE("Test IQmath mathematical functions", "[iqmath]")
{
    const float error_tolerance = 0.01;
    float res;

    _iq qA = _IQ(2.5);
    _iq qC;

    // Test square root
    qC = _IQsqrt(qA);
    res = _IQtoF(qC);
    ESP_LOGI(TAG, "Square root of %f = %f", _IQtoF(qA), res);
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(res, 1.58113885, error_tolerance));

    // Test trigonometric functions
    qA = _IQ(M_PI / 4.0); // 45 degrees

    // Test sin
    qC = _IQsin(qA);
    res = _IQtoF(qC);
    ESP_LOGI(TAG, "sin(pi/4) = %f", res);
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(res, 0.707106781, error_tolerance));

    // Test cos
    qC = _IQcos(qA);
    res = _IQtoF(qC);
    ESP_LOGI(TAG, "cos(pi/4) = %f", res);
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(res, 0.707106781, error_tolerance));
}

TEST_CASE("Test IQ8 type operations", "[iqmath]")
{
    const float error_tolerance = 0.01;
    float res;

    /* IQ variables using IQ8 type */
    _iq8 q8A = _IQ8(1.5);
    _iq8 q8B = _IQ8(2.5);
    _iq8 q8C;

    // Test IQ8 addition
    q8C = q8A + q8B;
    res = _IQ8toF(q8C);
    ESP_LOGI(TAG, "IQ8 Addition: %f + %f = %f", _IQ8toF(q8A), _IQ8toF(q8B), res);
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(res, 4.0, error_tolerance));

    // Test IQ8 multiplication
    q8C = _IQ8mpy(q8A, q8B);
    res = _IQ8toF(q8C);
    ESP_LOGI(TAG, "IQ8 Multiplication: %f * %f = %f", _IQ8toF(q8A), _IQ8toF(q8B), res);
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(res, 3.75, error_tolerance));
}

TEST_CASE("Test IQ conversion and saturation", "[iqmath]")
{
    const float error_tolerance = 0.01;
    float res;

    // Test float to IQ conversion
    float test_val = 3.14159;
    _iq iq_val = _IQ(test_val);
    res = _IQtoF(iq_val);
    ESP_LOGI(TAG, "Float to IQ conversion: %f -> %f", test_val, res);
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(res, test_val, error_tolerance));

    // Test IQ saturation
    _iq8 q8A = _IQ8(16.0);
    _iq qC = _IQ8toIQ(_IQsat(q8A, _IQtoQ8(MAX_IQ_POS), _IQtoQ8(MAX_IQ_NEG)));
    res = _IQtoF(qC);
    ESP_LOGI(TAG, "IQ saturation test: %f", res);
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(res, 16.0, error_tolerance));
}