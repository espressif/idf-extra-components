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

/* Tolerance of the float backend. */
#define TEST_EPS (1e-5f)

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

/*
 * The IQ backends are exercised in clarke_park_test.cpp. The case below only
 * checks that the C _Generic dispatch reaches the right backend when two
 * different Q-formats are used in the same translation unit, which is the
 * property that replaced CONFIG_CLARKE_PARK_IQ_FORMAT.
 */
TEST_CASE("C _Generic API: IQ dispatch picks the backend from the Q-format",
          "[clarke_park][generic][iq]")
{
    /* Q8 and Q15 coordinates in one translation unit, no GLOBAL_IQ involved */
    clarke_park_uvw_iq8_t uvw8 = { .u = _IQ8(1.0f), .v = _IQ8(-0.5f), .w = _IQ8(-0.5f) };
    clarke_park_uvw_iq15_t uvw15 = { .u = _IQ15(1.0f), .v = _IQ15(-0.5f), .w = _IQ15(-0.5f) };

    clarke_park_ab_iq8_t ab8 = { 0 };
    clarke_park_ab_iq15_t ab15 = { 0 };

    clarke_park_clarke(&uvw8, &ab8);
    clarke_park_clarke(&uvw15, &ab15);

    TEST_ASSERT_FLOAT_WITHIN(1e-2f, 1.0f, _IQ8toF(ab8.alpha));
    TEST_ASSERT_FLOAT_WITHIN(2e-3f, 1.0f, _IQ15toF(ab15.alpha));
}

TEST_CASE("C _Generic API: every Q-format backend", "[clarke_park][generic][iq][allformats]")
{
    /* Suffixed APIs, not the generic clarke_park_clarke() macro: iterating
     * CLARKE_PARK_IQ_FORMATS and then expanding that macro would re-enter
     * the same function-like list (preprocessor paint).
     *
     * Scale the raw IQ word rather than calling _IQNtoF: IQmath's converter
     * turns values just below 1.0 into 2.0 in Q25..Q29 on the host, so Park
     * at theta=0 (cos(0) = (1<<q)-1 from the IQ31 table) would fail a
     * working backend. Clarke of (1,-0.5,-0.5) is exact in high Q and only
     * loosely checked below Q8. Park at theta=0 needs cos(0)=1, which
     * IQmath cannot represent in Q1..Q7 after the IQ31 table shift, so it
     * is checked from Q8 up. */
#define CLARKE_PARK_TEST_IQ_TO_F(_q, _v) ((float)(_v) / (float)((int32_t)1 << (_q)))
#define CLARKE_PARK_IQ_SMOKE(_q)                                                         \
    do {                                                                                 \
        clarke_park_uvw_iq##_q##_t uvw = {                                               \
            .u = _IQ##_q(1.0f), .v = _IQ##_q(-0.5f), .w = _IQ##_q(-0.5f)                 \
        };                                                                               \
        clarke_park_ab_iq##_q##_t ab = { 0 };                                            \
        clarke_park_clarke_iq##_q(&uvw, &ab);                                            \
        TEST_ASSERT_FLOAT_WITHIN((_q) < 8 ? 0.6f : 2e-2f, 1.0f,                          \
                                 CLARKE_PARK_TEST_IQ_TO_F(_q, ab.alpha));                \
        clarke_park_dq_iq##_q##_t dq = { 0 };                                            \
        clarke_park_park_iq##_q(_IQ##_q(0.0f), &ab, &dq);                                \
        if ((_q) >= 8) {                                                                 \
            TEST_ASSERT_FLOAT_WITHIN(2e-2f, CLARKE_PARK_TEST_IQ_TO_F(_q, ab.alpha),       \
                                     CLARKE_PARK_TEST_IQ_TO_F(_q, dq.d));                \
            TEST_ASSERT_FLOAT_WITHIN(2e-2f, CLARKE_PARK_TEST_IQ_TO_F(_q, ab.beta),        \
                                     CLARKE_PARK_TEST_IQ_TO_F(_q, dq.q));                \
        }                                                                                \
        clarke_park_ab_iq##_q##_t ab_out = { 0 };                                        \
        clarke_park_ipark_iq##_q(_IQ##_q(0.0f), &dq, &ab_out);                           \
        if ((_q) >= 8) {                                                                 \
            TEST_ASSERT_FLOAT_WITHIN(2e-2f, CLARKE_PARK_TEST_IQ_TO_F(_q, ab.alpha),       \
                                     CLARKE_PARK_TEST_IQ_TO_F(_q, ab_out.alpha));        \
        }                                                                                \
        clarke_park_uvw_iq##_q##_t uvw_out = { 0 };                                      \
        clarke_park_iclarke_iq##_q(&ab, &uvw_out);                                       \
        TEST_ASSERT_FLOAT_WITHIN((_q) < 8 ? 0.6f : 2e-2f,                                \
                                 CLARKE_PARK_TEST_IQ_TO_F(_q, uvw.u),                     \
                                 CLARKE_PARK_TEST_IQ_TO_F(_q, uvw_out.u));                \
    } while (0);
    CLARKE_PARK_IQ_FORMATS(CLARKE_PARK_IQ_SMOKE)
#undef CLARKE_PARK_IQ_SMOKE
#undef CLARKE_PARK_TEST_IQ_TO_F
}
