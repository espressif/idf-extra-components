/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * clarke_park C++ host tests: IQmath backend.
 *
 * Every case in this file mirrors the equally named case of
 * clarke_park_test.c, which exercises the float backend through the C
 * _Generic API; here the IQmath backend is exercised through the C++
 * overloads. Keep both files in sync when adding or changing a case.
 */

#include <math.h>
#include "unity.h"
#include "clarke_park.h"

#define TEST_PI (3.14159265358979323846f)

/* Small angle, so the case also holds with small Q formats (e.g. Q15, where
 * the representable range is only [-1, 1)). */
#define TEST_THETA (0.5f)

/* Tolerance of the fixed-point backend. It is dominated by the Q format
 * quantization step and the precision of the IQmath sin/cos tables. */
#define TEST_IQ_EPS (2e-3f)

namespace {

/* Reference value of a float expression, computed in double precision: for
 * the smallest supported Q format (Q1) the float rounding of an expression
 * is already comparable to the quantization step. */
double iq_ref(double value)
{
    return value;
}

}  // namespace

/**
 * @brief Check that the three phase components sum to zero
 *
 * A valid U/V/W coordinate has no zero-sequence component: u + v + w == 0.
 */
static void test_assert_uvw_sum_is_zero(const clarke_park_uvw_iq_t *uvw, float eps)
{
    TEST_ASSERT_FLOAT_WITHIN(eps, 0.0f, _IQtoF(uvw->u + uvw->v + uvw->w));
}

TEST_CASE("IQmath Clarke transform", "[clarke_park][iq]")
{
    clarke_park_uvw_iq_t uvw = { .u = _IQ(1.0f), .v = _IQ(-0.5f), .w = _IQ(-0.5f) };
    clarke_park_ab_iq_t ab = {};

    clarke_park_clarke(&uvw, &ab);
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 1.0f, _IQtoF(ab.alpha));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.0f, _IQtoF(ab.beta));
}

TEST_CASE("IQmath inverse Clarke transform", "[clarke_park][iq]")
{
    clarke_park_ab_iq_t ab = { .alpha = _IQ(1.0f), .beta = _IQ(0.0f) };
    clarke_park_uvw_iq_t uvw = {};

    clarke_park_iclarke(&ab, &uvw);
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 1.0f, _IQtoF(uvw.u));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, -0.5f, _IQtoF(uvw.v));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, -0.5f, _IQtoF(uvw.w));
    /* Stationary (zero frequency) component is always zero */
    test_assert_uvw_sum_is_zero(&uvw, TEST_IQ_EPS);
}

TEST_CASE("IQmath Park transform", "[clarke_park][iq]")
{
    clarke_park_ab_iq_t ab = { .alpha = _IQ(0.6f), .beta = _IQ(0.3f) };
    clarke_park_dq_iq_t dq = {};

    float d_exp = iq_ref(0.6 * cos(TEST_THETA) + 0.3 * sin(TEST_THETA));
    float q_exp = iq_ref(-0.6 * sin(TEST_THETA) + 0.3 * cos(TEST_THETA));

    clarke_park_park(_IQ(TEST_THETA), &ab, &dq);
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, d_exp, _IQtoF(dq.d));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, q_exp, _IQtoF(dq.q));

    /* theta = 0 must be the identity transform */
    clarke_park_park(_IQ(0.0f), &ab, &dq);
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, _IQtoF(ab.alpha), _IQtoF(dq.d));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, _IQtoF(ab.beta), _IQtoF(dq.q));
}

TEST_CASE("IQmath inverse Park transform", "[clarke_park][iq]")
{
    clarke_park_dq_iq_t dq = { .d = _IQ(0.6f), .q = _IQ(0.3f) };
    clarke_park_ab_iq_t ab = {};

    float alpha_exp = iq_ref(0.6 * cos(TEST_THETA) - 0.3 * sin(TEST_THETA));
    float beta_exp = iq_ref(0.6 * sin(TEST_THETA) + 0.3 * cos(TEST_THETA));

    clarke_park_ipark(_IQ(TEST_THETA), &dq, &ab);
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, alpha_exp, _IQtoF(ab.alpha));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, beta_exp, _IQtoF(ab.beta));
}

TEST_CASE("IQmath Park roundtrip", "[clarke_park][iq]")
{
    clarke_park_ab_iq_t ab_in = { .alpha = _IQ(0.8f), .beta = _IQ(0.6f) };
    clarke_park_dq_iq_t dq = {};
    clarke_park_ab_iq_t ab_out = {};

    clarke_park_park(_IQ(TEST_THETA), &ab_in, &dq);
    clarke_park_ipark(_IQ(TEST_THETA), &dq, &ab_out);
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, _IQtoF(ab_in.alpha), _IQtoF(ab_out.alpha));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, _IQtoF(ab_in.beta), _IQtoF(ab_out.beta));
}

TEST_CASE("IQmath Clarke roundtrip", "[clarke_park][iq]")
{
    clarke_park_uvw_iq_t uvw_in = { .u = _IQ(1.0f), .v = _IQ(-0.2f), .w = _IQ(-0.8f) };
    clarke_park_ab_iq_t ab = {};
    clarke_park_uvw_iq_t uvw_out = {};

    clarke_park_clarke(&uvw_in, &ab);
    clarke_park_iclarke(&ab, &uvw_out);
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, _IQtoF(uvw_in.u), _IQtoF(uvw_out.u));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, _IQtoF(uvw_in.v), _IQtoF(uvw_out.v));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, _IQtoF(uvw_in.w), _IQtoF(uvw_out.w));
}
