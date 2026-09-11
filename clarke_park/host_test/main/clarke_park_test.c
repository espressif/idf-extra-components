/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * clarke_park C host tests: float backend.
 *
 * The cases in this file are mirrored one-to-one by clarke_park_test.cpp,
 * which exercises the IQmath backend through the C++ overloads. Keep both
 * files in sync when adding or changing a case.
 */

#include <math.h>
#include "unity.h"
#include "clarke_park.h"

#define TEST_PI (3.14159265358979323846f)

/* Small angle, so the case also holds with small Q formats (e.g. Q15, where
 * the representable range is only [-1, 1)). */
#define TEST_THETA (0.5f)

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
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, ab.alpha);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, ab.beta);
}

TEST_CASE("float inverse Clarke transform", "[clarke_park][float]")
{
    clarke_park_ab_f_t ab = { .alpha = 1.0f, .beta = 0.0f };
    clarke_park_uvw_f_t uvw = { 0 };

    clarke_park_iclarke(&ab, &uvw);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, uvw.u);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -0.5f, uvw.v);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -0.5f, uvw.w);
    /* Stationary (zero frequency) component is always zero */
    test_assert_uvw_sum_is_zero(&uvw, 1e-5f);
}

TEST_CASE("float Park transform", "[clarke_park][float]")
{
    clarke_park_ab_f_t ab = { .alpha = 0.6f, .beta = 0.3f };
    clarke_park_dq_f_t dq = { 0 };

    float d_exp = 0.6f * cosf(TEST_THETA) + 0.3f * sinf(TEST_THETA);
    float q_exp = -0.6f * sinf(TEST_THETA) + 0.3f * cosf(TEST_THETA);

    clarke_park_park(TEST_THETA, &ab, &dq);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, d_exp, dq.d);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, q_exp, dq.q);

    /* theta = 0 must be the identity transform */
    clarke_park_park(0.0f, &ab, &dq);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, ab.alpha, dq.d);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, ab.beta, dq.q);
}

TEST_CASE("float inverse Park transform", "[clarke_park][float]")
{
    clarke_park_dq_f_t dq = { .d = 0.6f, .q = 0.3f };
    clarke_park_ab_f_t ab = { 0 };

    float alpha_exp = 0.6f * cosf(TEST_THETA) - 0.3f * sinf(TEST_THETA);
    float beta_exp = 0.6f * sinf(TEST_THETA) + 0.3f * cosf(TEST_THETA);

    clarke_park_ipark(TEST_THETA, &dq, &ab);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, alpha_exp, ab.alpha);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, beta_exp, ab.beta);
}

TEST_CASE("float Park roundtrip", "[clarke_park][float]")
{
    clarke_park_ab_f_t ab_in = { .alpha = 0.8f, .beta = 0.6f };
    clarke_park_dq_f_t dq = { 0 };
    clarke_park_ab_f_t ab_out = { 0 };

    clarke_park_park(TEST_THETA, &ab_in, &dq);
    clarke_park_ipark(TEST_THETA, &dq, &ab_out);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, ab_in.alpha, ab_out.alpha);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, ab_in.beta, ab_out.beta);
}

TEST_CASE("float Clarke roundtrip", "[clarke_park][float]")
{
    clarke_park_uvw_f_t uvw_in = { .u = 1.0f, .v = -0.2f, .w = -0.8f };
    clarke_park_ab_f_t ab = { 0 };
    clarke_park_uvw_f_t uvw_out = { 0 };

    clarke_park_clarke(&uvw_in, &ab);
    clarke_park_iclarke(&ab, &uvw_out);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, uvw_in.u, uvw_out.u);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, uvw_in.v, uvw_out.v);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, uvw_in.w, uvw_out.w);
}
