/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * clarke_park C host tests: float algorithm via the C _Generic API.
 * IQmath is a single Q8/Q15 dispatch smoke; the fixed-point algorithm
 * and every-Q backends live in clarke_park_test.cpp.
 */

#include <math.h>
#include "unity.h"
#include "clarke_park.h"

/* Small angle, so the case also holds with small Q formats (e.g. Q15, where
 * the representable range is only [-1, 1)). */
#define TEST_THETA (0.5f)
#define TEST_HALF_PI (1.57079632679489661923f)

/* Tolerance of the float backend. */
#define TEST_EPS (1e-5f)
/* Q15 and finer, including the unsuffixed _iq backend. */
#define TEST_IQ_EPS (2e-3f)
/* Q8 step is 1/256; Clarke of (1, -0.5, -0.5) is already ~4e-3 off. */
#define TEST_IQ8_EPS (1e-2f)

/**
 * @brief Check that the three phase components sum to zero
 *
 * A valid U/V/W coordinate has no zero-sequence component: u + v + w == 0.
 */
static void test_assert_uvw_sum_is_zero(const clarke_park_uvw_f_t *uvw, float eps)
{
    TEST_ASSERT_FLOAT_WITHIN(eps, 0.0f, uvw->u + uvw->v + uvw->w);
}

TEST_CASE("float Clarke transform", "[clarke_park][float]")
{
    clarke_park_uvw_f_t uvw = { .u = 1.0f, .v = -0.5f, .w = -0.5f };
    clarke_park_ab_f_t ab = { 0 };

    clarke_park_clarke(&uvw, &ab);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, 1.0f, ab.alpha);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, 0.0f, ab.beta);

    /* beta = (v - w) / sqrt(3); (0, 1, -1) must not collapse to the alpha axis */
    uvw = (clarke_park_uvw_f_t) {
        .u = 0.0f, .v = 1.0f, .w = -1.0f
    };
    clarke_park_clarke(&uvw, &ab);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, 0.0f, ab.alpha);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, 2.0f / sqrtf(3.0f), ab.beta);
}

TEST_CASE("float inverse Clarke transform", "[clarke_park][float]")
{
    clarke_park_ab_f_t ab = { .alpha = 1.0f, .beta = 0.0f };
    clarke_park_uvw_f_t uvw = { 0 };

    clarke_park_iclarke(&ab, &uvw);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, 1.0f, uvw.u);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, -0.5f, uvw.v);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, -0.5f, uvw.w);
    /* Stationary (zero frequency) component is always zero */
    test_assert_uvw_sum_is_zero(&uvw, TEST_EPS);
}

TEST_CASE("float Park transform", "[clarke_park][float]")
{
    clarke_park_ab_f_t ab = { .alpha = 0.6f, .beta = 0.3f };
    clarke_park_dq_f_t dq = { 0 };

    float d_exp = 0.6f * cosf(TEST_THETA) + 0.3f * sinf(TEST_THETA);
    float q_exp = -0.6f * sinf(TEST_THETA) + 0.3f * cosf(TEST_THETA);

    clarke_park_park(TEST_THETA, &ab, &dq);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, d_exp, dq.d);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, q_exp, dq.q);

    /* theta = 0 must be the identity transform */
    clarke_park_park(0.0f, &ab, &dq);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, ab.alpha, dq.d);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, ab.beta, dq.q);

    /* theta = pi/2: (1, 0) -> (0, -1), locks the rotation sense */
    ab = (clarke_park_ab_f_t) {
        .alpha = 1.0f, .beta = 0.0f
    };
    clarke_park_park(TEST_HALF_PI, &ab, &dq);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, 0.0f, dq.d);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, -1.0f, dq.q);
}

TEST_CASE("float inverse Park transform", "[clarke_park][float]")
{
    clarke_park_dq_f_t dq = { .d = 0.6f, .q = 0.3f };
    clarke_park_ab_f_t ab = { 0 };

    float alpha_exp = 0.6f * cosf(TEST_THETA) - 0.3f * sinf(TEST_THETA);
    float beta_exp = 0.6f * sinf(TEST_THETA) + 0.3f * cosf(TEST_THETA);

    clarke_park_ipark(TEST_THETA, &dq, &ab);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, alpha_exp, ab.alpha);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, beta_exp, ab.beta);
}

TEST_CASE("float Park roundtrip", "[clarke_park][float]")
{
    clarke_park_ab_f_t ab_in = { .alpha = 0.8f, .beta = 0.6f };
    clarke_park_dq_f_t dq = { 0 };
    clarke_park_ab_f_t ab_out = { 0 };

    clarke_park_park(TEST_THETA, &ab_in, &dq);
    clarke_park_ipark(TEST_THETA, &dq, &ab_out);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, ab_in.alpha, ab_out.alpha);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, ab_in.beta, ab_out.beta);
}

TEST_CASE("float Clarke roundtrip", "[clarke_park][float]")
{
    clarke_park_uvw_f_t uvw_in = { .u = 1.0f, .v = -0.2f, .w = -0.8f };
    clarke_park_ab_f_t ab = { 0 };
    clarke_park_uvw_f_t uvw_out = { 0 };

    clarke_park_clarke(&uvw_in, &ab);
    clarke_park_iclarke(&ab, &uvw_out);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, uvw_in.u, uvw_out.u);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, uvw_in.v, uvw_out.v);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, uvw_in.w, uvw_out.w);
}

TEST_CASE("C _Generic API: IQ dispatch picks the backend from the Q-format",
          "[clarke_park][generic][iq]")
{
    /* IQmath algorithm coverage lives in clarke_park_test.cpp. This case
     * only pins down that _Generic picks Q8 vs Q15 from the coordinate. */
    clarke_park_uvw_iq8_t uvw8 = { .u = _IQ8(1.0f), .v = _IQ8(-0.5f), .w = _IQ8(-0.5f) };
    clarke_park_uvw_iq15_t uvw15 = { .u = _IQ15(1.0f), .v = _IQ15(-0.5f), .w = _IQ15(-0.5f) };
    clarke_park_ab_iq8_t ab8 = { 0 };
    clarke_park_ab_iq15_t ab15 = { 0 };

    clarke_park_clarke(&uvw8, &ab8);
    clarke_park_clarke(&uvw15, &ab15);
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ8_EPS, 1.0f, _IQ8toF(ab8.alpha));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ8_EPS, 0.0f, _IQ8toF(ab8.beta));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 1.0f, _IQ15toF(ab15.alpha));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.0f, _IQ15toF(ab15.beta));
}
