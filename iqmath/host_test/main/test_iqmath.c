/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * IQmath host tests: pure fixed-point math, no ESP chip required.
 *
 * Tests are grouped by tag:
 *   [conversion]   - type conversions and string conversions
 *   [arithmetic]   - basic arithmetic operations
 *   [trigonometry] - trigonometric and inverse trigonometric functions
 *   [hyperbolic]   - exponential, logarithmic and square-root functions
 *   [saturation]   - saturation, limits and integer/fractional part handling
 *
 * 64-bit Linux widens int_fast32_t/uint_fast32_t to 64 bits, which breaks
 * the 32-bit wrap assumed by some TI routines. Host coverage is limited:
 *   - _IQtoa: integer-only formats; fractional digits are wrong on host
 *   - _IQexp: non-negative inputs only (negatives return 0 on host)
 *   - _IQlog: power-of-two inputs only (mantissa path is wrong on host)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "unity.h"
#include "IQmathLib.h"

/* Relative tolerance helper: |result - expected| <= tolerance * |expected| */
#define ERROR_WITHIN_TOLERANCE(result, expected, tolerance) \
    (((result) >= ((expected) - (fabs((expected)) * (tolerance)))) && \
    ((result) <= ((expected) + (fabs((expected)) * (tolerance)))))

/* Absolute tolerance helper for values around zero */
#define ERROR_WITHIN_ABS_TOLERANCE(result, expected, tolerance) \
    (fabs((result) - (expected)) <= (tolerance))

static const float error_tolerance = 0.01f;

/* --------------------------------------------------------------------------
 * Conversion tests: float/IQ/string conversions and format casting
 * -------------------------------------------------------------------------- */

TEST_CASE("Test float to IQ and IQ to float conversion", "[conversion]")
{
    const float test_vals[] = {0.0f, 0.5f, -0.5f, 1.5f, -1.5f, 3.14159f, -3.14159f, 123.456f, -123.456f};

    for (size_t i = 0; i < sizeof(test_vals) / sizeof(test_vals[0]); i++) {
        float res = _IQtoF(_IQ(test_vals[i]));
        TEST_ASSERT_FLOAT_WITHIN(0.001f, test_vals[i], res);
    }
}

TEST_CASE("Test IQ conversion between different Q formats", "[conversion]")
{
    /* Global IQ is 24. Test conversions between IQ24 and other formats. */
    _iq qA = _IQ(1.5);

    /* IQ24 -> IQ30 -> IQ24 */
    _iq30 q30 = _IQtoIQ30(qA);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.5f, _IQ30toF(q30));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.5f, _IQtoF(_IQ30toIQ(q30)));

    /* IQ24 -> IQ16 -> IQ24 */
    _iq16 q16 = _IQtoIQ16(qA);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.5f, _IQ16toF(q16));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.5f, _IQtoF(_IQ16toIQ(q16)));

    /* IQ24 -> IQ8 -> IQ24 */
    _iq8 q8 = _IQtoIQ8(qA);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.5f, _IQ8toF(q8));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.5f, _IQtoF(_IQ8toIQ(q8)));

    /* Negative values survive the round trip */
    q8 = _IQtoIQ8(_IQ(-1.5));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.5f, _IQtoF(_IQ8toIQ(q8)));
}

TEST_CASE("Test explicit IQ type to float conversion", "[conversion]")
{
    /* IQ8 resolution is 1/256 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f / 256.0f, _IQ8toF(1));

    _iq8 q8A = _IQ8(2.5);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.5f, _IQ8toF(q8A));

    _iq16 q16A = _IQ16(-0.25);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -0.25f, _IQ16toF(q16A));
}

TEST_CASE("Test atoIQ string conversion", "[conversion]")
{
    /* _atoIQN converts a string to the specified IQ format */
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.5f, _IQ24toF(_atoIQ24("1.5")));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -0.25f, _IQ24toF(_atoIQ24("-0.25")));
    TEST_ASSERT_EQUAL_INT32(0, _atoIQ24("0"));

    /* Global format wrapper */
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.14159f, _IQtoF(_atoIQ("3.14159")));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.1f, _IQtoF(_atoIQ("0.1")));
}

TEST_CASE("Test IQtoa integer format", "[conversion]")
{
    char buf[16];

    /* Fractional digits are incorrect on 64-bit hosts: uint_fast32_t is
     * 64-bit, so the IQ32 left-shift in __IQNtoa does not wrap. Integer-only
     * formats still work. */
    TEST_ASSERT_EQUAL_INT16(0, _IQtoa(buf, "%4.0f", _IQ(1.5)));
    TEST_ASSERT_EQUAL_STRING("0001", buf);

    TEST_ASSERT_EQUAL_INT16(0, _IQtoa(buf, "%4.0f", _IQ(-2.25)));
    TEST_ASSERT_EQUAL_STRING("-0002", buf);

    TEST_ASSERT_EQUAL_INT16(0, _IQtoa(buf, "%4.0f", _IQ(123.456)));
    TEST_ASSERT_EQUAL_STRING("0123", buf);

    TEST_ASSERT_EQUAL_INT16(0, _IQtoa(buf, "%4.0f", _IQ(0.0)));
    TEST_ASSERT_EQUAL_STRING("0000", buf);

    TEST_ASSERT_EQUAL_INT16(2, _IQtoa(buf, "4.0f", _IQ(1.0)));
    TEST_ASSERT_EQUAL_INT16(1, _IQtoa(buf, "%1.0f", _IQ(12.0)));
}

/* --------------------------------------------------------------------------
 * Arithmetic tests: multiply, divide, rounding-multiply, power-of-two scaling
 * -------------------------------------------------------------------------- */

TEST_CASE("Test IQ basic arithmetic add and subtract", "[arithmetic]")
{
    _iq qA = _IQ(1.5);
    _iq qB = _IQ(2.5);
    _iq qC;

    qC = qA + qB;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, _IQtoF(qC));

    qC = qB - qA;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, _IQtoF(qC));

    /* Negative operands */
    qC = _IQ(-3.5) + qA;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -2.0f, _IQtoF(qC));
}

TEST_CASE("Test IQ multiply", "[arithmetic]")
{
    _iq qA = _IQ(1.5);
    _iq qB = _IQ(2.5);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.75f, _IQtoF(_IQmpy(qA, qB)));

    /* Negative operand */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.75f, _IQtoF(_IQmpy(_IQ(-1.5), qB)));

    /* _IQmpyI32: IQ multiplied by int32 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.5f, _IQtoF(_IQmpyI32(qA, 3)));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.0f, _IQtoF(_IQmpyI32(qA, -2)));

    /* _IQmpyI32int / _IQmpyI32frac: integer and fractional parts */
    TEST_ASSERT_EQUAL_INT32(4, _IQmpyI32int(qA, 3));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, _IQtoF(_IQmpyI32frac(qA, 3)));
}

TEST_CASE("Test IQ rounding multiply", "[arithmetic]")
{
    /* 0.2 * 0.3 has a discarded IQ24 fraction >= 0.5 ULP, so truncate and
     * round-to-nearest differ by exactly one bit. */
    _iq qA = _IQ(0.2);
    _iq qB = _IQ(0.3);
    _iq qMpy = _IQmpy(qA, qB);
    _iq qRmpy = _IQrmpy(qA, qB);
    _iq qRsmpy = _IQrsmpy(qA, qB);

    TEST_ASSERT_EQUAL_INT32(qMpy + 1, qRmpy);
    TEST_ASSERT_EQUAL_INT32(qRmpy, qRsmpy);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.06f, _IQtoF(qRmpy));
}

TEST_CASE("Test IQ divide", "[arithmetic]")
{
    _iq qB = _IQ(1.5);

    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQdiv(_IQ(2.5), qB)), 1.666667, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQdiv(_IQ(-2.5), qB)), -1.666667, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQdiv(_IQ(1.5), _IQ(2.5))), 0.6, error_tolerance));

    /* Zero numerator */
    TEST_ASSERT_EQUAL_INT32(0, _IQdiv(_IQ(0.0), qB));
}

TEST_CASE("Test IQ multiply and divide by power of two", "[arithmetic]")
{
    _iq qA = _IQ(1.25);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, _IQtoF(_IQmpy2(qA)));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, _IQtoF(_IQmpy4(qA)));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, _IQtoF(_IQmpy8(qA)));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, _IQtoF(_IQmpy16(qA)));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 40.0f, _IQtoF(_IQmpy32(qA)));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 80.0f, _IQtoF(_IQmpy64(qA)));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.625f, _IQtoF(_IQdiv2(qA)));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.3125f, _IQtoF(_IQdiv4(qA)));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.15625f, _IQtoF(_IQdiv8(qA)));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.25f, _IQtoF(_IQdiv32(_IQ(40.0))));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.25f, _IQtoF(_IQdiv64(_IQ(80.0))));
}

TEST_CASE("Test IQmpyIQX cross format multiply", "[arithmetic]")
{
    /* Multiply two values in different Q formats with the result in global format.
     * _IQmpyIQX(A, n1, B, n2): A is in Q(n1), B is in Q(n2). */
    _iq30 q30A = _IQ30(0.5);
    _iq8 q8B = _IQ8(2.0);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, _IQtoF(_IQmpyIQX(q30A, 30, q8B, 8)));

    q30A = _IQ30(-0.25);
    q8B = _IQ8(4.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, _IQtoF(_IQmpyIQX(q30A, 30, q8B, 8)));
}

/* --------------------------------------------------------------------------
 * Trigonometry tests: sin, cos, PU variants, asin, acos, atan, atan2
 * -------------------------------------------------------------------------- */

TEST_CASE("Test IQ sin and cos", "[trigonometry]")
{
    /* Special angles in radians */
    TEST_ASSERT(ERROR_WITHIN_ABS_TOLERANCE(_IQtoF(_IQsin(_IQ(0.0))), 0.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQcos(_IQ(0.0))), 1.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQsin(_IQ(M_PI / 6.0))), 0.5, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQcos(_IQ(M_PI / 6.0))), 0.866025404, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQsin(_IQ(M_PI / 4.0))), 0.707106781, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQcos(_IQ(M_PI / 4.0))), 0.707106781, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQsin(_IQ(M_PI / 3.0))), 0.866025404, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQcos(_IQ(M_PI / 3.0))), 0.5, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQsin(_IQ(M_PI / 2.0))), 1.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_ABS_TOLERANCE(_IQtoF(_IQcos(_IQ(M_PI / 2.0))), 0.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQsin(_IQ(M_PI))), 0.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQcos(_IQ(M_PI))), -1.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQsin(_IQ(3.0 * M_PI / 2.0))), -1.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQcos(_IQ(2.0 * M_PI))), 1.0, error_tolerance));

    /* Negative angles */
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQsin(_IQ(-M_PI / 4.0))), -0.707106781, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQcos(_IQ(-M_PI / 4.0))), 0.707106781, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQsin(_IQ(-M_PI / 2.0))), -1.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQcos(_IQ(-M_PI))), -1.0, error_tolerance));
}

TEST_CASE("Test IQ sinPU and cosPU per unit input", "[trigonometry]")
{
    /* PU variants take the input in per-unit cycles: 0.25 cycle = pi/2 */
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQsinPU(_IQ(0.25))), 1.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_ABS_TOLERANCE(_IQtoF(_IQcosPU(_IQ(0.25))), 0.0, error_tolerance));

    /* Half cycle = pi -> sin = 0, cos = -1 */
    TEST_ASSERT(ERROR_WITHIN_ABS_TOLERANCE(_IQtoF(_IQsinPU(_IQ(0.5))), 0.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQcosPU(_IQ(0.5))), -1.0, error_tolerance));
}

TEST_CASE("Test IQ asin and acos", "[trigonometry]")
{
    TEST_ASSERT(ERROR_WITHIN_ABS_TOLERANCE(_IQtoF(_IQasin(_IQ(0.0))), 0.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQasin(_IQ(0.5))), M_PI / 6.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQasin(_IQ(-0.5))), -M_PI / 6.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQasin(_IQ(0.7071))), M_PI / 4.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQasin(_IQ(1.0))), M_PI / 2.0, error_tolerance));

    TEST_ASSERT(ERROR_WITHIN_ABS_TOLERANCE(_IQtoF(_IQacos(_IQ(1.0))), 0.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQacos(_IQ(0.5))), M_PI / 3.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQacos(_IQ(0.0))), M_PI / 2.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQacos(_IQ(0.7071))), M_PI / 4.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQacos(_IQ(-1.0))), M_PI, error_tolerance));
}

TEST_CASE("Test IQ atan and atan2 four quadrant arctangent", "[trigonometry]")
{
    /* atan */
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQatan(_IQ(0.5))), 0.463647604, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQatan(_IQ(1.0))), M_PI / 4.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQatan(_IQ(-1.0))), -M_PI / 4.0, error_tolerance));

    /* atan2 quadrants */
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQatan2(_IQ(1.0), _IQ(1.0))), M_PI / 4.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQatan2(_IQ(1.0), _IQ(-1.0))), 3.0 * M_PI / 4.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQatan2(_IQ(-1.0), _IQ(-1.0))), -3.0 * M_PI / 4.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQatan2(_IQ(-1.0), _IQ(1.0))), -M_PI / 4.0, error_tolerance));

    /* On the +x axis: atan2(0, 1) = 0 */
    TEST_ASSERT(ERROR_WITHIN_ABS_TOLERANCE(_IQtoF(_IQatan2(_IQ(0.0), _IQ(1.0))), 0.0, error_tolerance));

    /* PU variant: result in cycles, pi/4 = 0.125 cycle */
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQatan2PU(_IQ(1.0), _IQ(1.0))), 0.125, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQatan2PU(_IQ(1.0), _IQ(-1.0))), 0.375, error_tolerance));
}

/* --------------------------------------------------------------------------
 * Hyperbolic tests: exp, log, sqrt, isqrt and magnitude
 * -------------------------------------------------------------------------- */

TEST_CASE("Test IQ square root and inverse square root", "[hyperbolic]")
{
    TEST_ASSERT(ERROR_WITHIN_ABS_TOLERANCE(_IQtoF(_IQsqrt(_IQ(0.0))), 0.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQsqrt(_IQ(1.0))), 1.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQsqrt(_IQ(0.25))), 0.5, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQsqrt(_IQ(2.0))), 1.414213562, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQsqrt(_IQ(2.5))), 1.58113885, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQsqrt(_IQ(4.0))), 2.0, error_tolerance));

    /* _IQisqrt(x) = 1 / sqrt(x) */
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQisqrt(_IQ(1.0))), 1.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQisqrt(_IQ(0.25))), 2.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQisqrt(_IQ(2.0))), 0.707106781, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQisqrt(_IQ(4.0))), 0.5, error_tolerance));
}

TEST_CASE("Test IQ magnitude", "[hyperbolic]")
{
    /* _IQmag computes sqrt(A^2 + B^2) on IQ31 inputs, result is IQ31.
     * Only fractional results (< 1.0) are valid. */
    int32_t a = (int32_t)(0.5 * 2147483647.0);
    int32_t b = (int32_t)(0.5 * 2147483647.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.707106781f, (float)_IQmag(a, b) / 2147483648.0f);

    a = (int32_t)(0.3 * 2147483647.0);
    b = (int32_t)(0.4 * 2147483647.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, (float)_IQmag(a, b) / 2147483648.0f);
}

TEST_CASE("Test IQ exponential", "[hyperbolic]")
{
    /* Negative inputs return 0 on 64-bit hosts: uint_fast32_t is 64-bit, so
     * the Taylor path's 32-bit unsigned wrap and the _IQNexp_min table no
     * longer match the original 32-bit assumptions. */
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQexp(_IQ(0.0))), 1.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQexp(_IQ(0.5))), 1.64872134, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQexp(_IQ(1.0))), 2.718281828, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQexp(_IQ(2.3))), 9.974182, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQexp(_IQ(3.0))), 20.085537, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQexp(_IQ(4.0))), 54.598150, error_tolerance));
}

TEST_CASE("Test IQ logarithm", "[hyperbolic]")
{
    /* Only power-of-two inputs are tested. On 64-bit hosts the mantissa
     * path is wrong because uint_fast32_t is 64-bit; powers of two skip
     * that path and only add exp * ln(2). */
    TEST_ASSERT(ERROR_WITHIN_ABS_TOLERANCE(_IQtoF(_IQlog(_IQ(1.0))), 0.0, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQlog(_IQ(2.0))), 0.693147181, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQlog(_IQ(4.0))), 1.386294361, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQlog(_IQ(0.5))), -0.693147181, error_tolerance));
    TEST_ASSERT(ERROR_WITHIN_TOLERANCE(_IQtoF(_IQlog(_IQ(0.25))), -1.386294361, error_tolerance));
}

/* --------------------------------------------------------------------------
 * Saturation tests: _IQsat clamps values to the given range
 * -------------------------------------------------------------------------- */

TEST_CASE("Test IQsat saturation", "[saturation]")
{
    /* Value within range is unchanged */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, _IQtoF(_IQsat(_IQ(1.5), _IQ(2.0), _IQ(-2.0))));

    /* Value above positive limit is clamped */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, _IQtoF(_IQsat(_IQ(3.0), _IQ(2.0), _IQ(-2.0))));

    /* Value below negative limit is clamped */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -2.0f, _IQtoF(_IQsat(_IQ(-3.0), _IQ(2.0), _IQ(-2.0))));

    /* Value equal to the limits is unchanged */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, _IQtoF(_IQsat(_IQ(2.0), _IQ(2.0), _IQ(-2.0))));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -2.0f, _IQtoF(_IQsat(_IQ(-2.0), _IQ(2.0), _IQ(-2.0))));

    /* Explicit IQ8 type */
    _iq8 q8Pos = _IQ8(2.0);
    _iq8 q8Neg = _IQ8(-2.0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, _IQ8toF(_IQsat(_IQ8(1.0), q8Pos, q8Neg)));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, _IQ8toF(_IQsat(_IQ8(16.0), q8Pos, q8Neg)));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -2.0f, _IQ8toF(_IQsat(_IQ8(-16.0), q8Pos, q8Neg)));
}

TEST_CASE("Test IQint and IQfrac integer and fractional parts", "[saturation]")
{
    _iq qA = _IQ(3.75);

    TEST_ASSERT_EQUAL_INT32(3, _IQint(qA));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.75f, _IQtoF(_IQfrac(qA)));

    /* _IQint truncates towards negative infinity.
     * _IQfrac returns the two's-complement fractional remainder:
     * -3.75 = -4 + 0.25 */
    qA = _IQ(-3.75);
    TEST_ASSERT_EQUAL_INT32(-4, _IQint(qA));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.25f, _IQtoF(_IQfrac(qA)));
}

TEST_CASE("Test IQabs", "[saturation]")
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, _IQtoF(_IQabs(_IQ(1.5))));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, _IQtoF(_IQabs(_IQ(-1.5))));
    TEST_ASSERT_EQUAL_INT32(0, _IQabs(_IQ(0.0)));
}
