/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "esp_log.h"
#include "IQmathLib.h"

static const char *TAG = "example";

#define ERROR_WITHIN_TOLERANCE(result, expected, tolerance) \
    (((result) >= ((expected) - ((result) * (tolerance)))) && ((result) <= ((expected) + ((result) * (tolerance)))))

void app_main(void)
{
    const float error_tolerance = 0.01;
    bool test_failure = false;

    /* floating point variable to verify results */
    float res;
    /* IQ variables using global type */
    _iq qA, qB, qC;
    /* IQ variables using IQ8 type */
    _iq8 q8A, q8B, q8C;
    /* IQ variables using IQ15 type */
    _iq15 q15A, q15C;

    /* Basic global IQ operations. */
    qA  = _IQ(1.0);
    qB  = _IQ(2.5);
    qC  = qA + qB;
    /* 3.5 = 1.0 + 2.5 */
    res = _IQtoF(qC);
    if (!ERROR_WITHIN_TOLERANCE(res, 3.5, error_tolerance)) {
        test_failure = true;
    };

    qC  = qC - _IQmpy2(qA);
    /* 1.5 = 3.5 - 2*(1.0) */
    res = _IQtoF(qC);
    if (!ERROR_WITHIN_TOLERANCE(res, 1.5, error_tolerance)) {
        test_failure = true;
    };

    qC  = _IQmpy(qB, qC);
    /* 3.75 = 2.5 * 1.5 */
    res = _IQtoF(qC);
    if (!ERROR_WITHIN_TOLERANCE(res, 3.75, error_tolerance)) {
        test_failure = true;
    };

    qC  = _IQdiv(qC, qB);
    /* 1.5 = 3.75 / 2.5 */
    res = _IQtoF(qC);
    if (!ERROR_WITHIN_TOLERANCE(res, 1.5, error_tolerance)) {
        test_failure = true;
    };

    qC  = _IQsqrt(qB);
    /* 1.58113885 = sqrt(2.5) */
    res = _IQtoF(qC);
    if (!ERROR_WITHIN_TOLERANCE(res, 1.58113885, error_tolerance)) {
        test_failure = true;
    };

    /* Trigonometric global IQ operations. */
    qA  = _IQ(M_PI / 4.0);
    qB  = _IQ(0.5);
    qC  = _IQsin(qA);
    /* 0.707106709 = sin(PI/4) */
    res = _IQtoF(qC);
    if (!ERROR_WITHIN_TOLERANCE(res, 0.707106709, error_tolerance)) {
        test_failure = true;
    };

    qC  = _IQcos(qA);
    /* 0.707106769 = cos(PI/4) */
    res = _IQtoF(qC);
    if (!ERROR_WITHIN_TOLERANCE(res, 0.707106769, error_tolerance)) {
        test_failure = true;
    };

    qC  = _IQatan(qB);
    /* 0.463647604 = atan(0.5) */
    res = _IQtoF(qC);
    if (!ERROR_WITHIN_TOLERANCE(res, 0.463647604, error_tolerance)) {
        test_failure = true;
    };

    /* Exponential global IQ operations. */
    qA  = _IQ(2.71828);
    qB  = _IQ(0.5);
    qC  = _IQlog(qA);
    /* 0.9999999225 = ln(e) */
    res = _IQtoF(qC);
    if (!ERROR_WITHIN_TOLERANCE(res, 0.9999999225, error_tolerance)) {
        test_failure = true;
    };

    qC  = _IQexp(qB);
    /* 1.64872134 = e^0.5 */
    res = _IQtoF(qC);
    if (!ERROR_WITHIN_TOLERANCE(res, 1.64872134, error_tolerance)) {
        test_failure = true;
    };

    /* Basic explicit type IQ8 operations. */
    q8A = _IQ8(1.0);
    q8B = _IQ8(2.5);
    q8C = q8A + q8B;
    /* 3.5 = 1.0 + 2.5 */
    res = _IQ8toF(q8C);
    if (!ERROR_WITHIN_TOLERANCE(res, 3.5, error_tolerance)) {
        test_failure = true;
    };

    q8C = q8C - _IQmpy2(q8A);
    /* 1.5 = 3.5 - 2*(1.0) */
    res = _IQ8toF(q8C);
    if (!ERROR_WITHIN_TOLERANCE(res, 1.5, error_tolerance)) {
        test_failure = true;
    };

    q8C = _IQ8mpy(q8B, q8C);
    /* 3.75 = 2.5 * 1.5 */
    res = _IQ8toF(q8C);
    if (!ERROR_WITHIN_TOLERANCE(res, 3.75, error_tolerance)) {
        test_failure = true;
    };

    q8C = _IQ8div(q8C, q8B);
    /* 1.5 = 3.75 / 2.5 */
    res = _IQ8toF(q8C);
    if (!ERROR_WITHIN_TOLERANCE(res, 1.5, error_tolerance)) {
        test_failure = true;
    };

    q8C = _IQ8sqrt(q8B);
    /* 1.58203125 = sqrt(2.5) */
    res = _IQ8toF(q8C);
    if (!ERROR_WITHIN_TOLERANCE(res, 1.58203125, error_tolerance)) {
        test_failure = true;
    };

    /* Trigonometric explicit type IQ15 operations. */
    q15A = _IQ15(M_PI / 4.0);
    q15C = _IQ15sin(q15A);
    /* 0.707061768 = sin(PI/4) */
    res  = _IQ15toF(q15C);
    if (!ERROR_WITHIN_TOLERANCE(res, 0.707061768, error_tolerance)) {
        test_failure = true;
    };

    q15C = _IQ15cos(q15A);
    /* 0.707061768 = cos(PI/4) */
    res  = _IQ15toF(q15C);
    if (!ERROR_WITHIN_TOLERANCE(res, 0.707061768, error_tolerance)) {
        test_failure = true;
    };

    /* Explicit type IQ8 to Global IQ conversion with saturation check. */
    q8A = _IQ8(1.0);
    q8B = _IQ8(16.0);
    qC  = _IQ8toIQ(_IQsat(q8A, _IQtoQ8(MAX_IQ_POS), _IQtoQ8(MAX_IQ_NEG)));
    /* _IQ8(1.0) -> _IQ(1.0) (q8A does not saturate) */
    res = _IQtoF(qC);
    if (!ERROR_WITHIN_TOLERANCE(res, 1.0, error_tolerance)) {
        test_failure = true;
    };

    qC  = _IQ8toIQ(_IQsat(q8B, _IQtoQ8(MAX_IQ_POS), _IQtoQ8(MAX_IQ_NEG)));
    /* _IQ8(16.0) -> ~MAX_IQ_POS (q8A saturates to maximum positive _IQ value) */
    res = _IQtoF(qC);
    if (!ERROR_WITHIN_TOLERANCE(res, 16.0, error_tolerance)) {
        test_failure = true;
    };

    if (test_failure == true) {
        ESP_LOGE(TAG, "IQMath test failed");
    } else {
        ESP_LOGI(TAG, "IQMath test passed");
    }
}
