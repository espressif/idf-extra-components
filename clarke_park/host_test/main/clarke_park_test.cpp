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

    /* Reference values are computed in double precision: for the smallest
     * supported Q format (Q1) the float rounding of an expression is already
     * comparable to the quantization step. */
    const double d_exp = 0.6 * cos(TEST_THETA) + 0.3 * sin(TEST_THETA);
    const double q_exp = -0.6 * sin(TEST_THETA) + 0.3 * cos(TEST_THETA);

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

    /* Reference values are computed in double precision, see the Park
     * transform test case above. */
    const double alpha_exp = 0.6 * cos(TEST_THETA) - 0.3 * sin(TEST_THETA);
    const double beta_exp = 0.6 * sin(TEST_THETA) + 0.3 * cos(TEST_THETA);

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

/*
 * The point of the typed coordinate structs: several Q-formats coexist in one
 * firmware, each dispatching to its own backend, without any GLOBAL_IQ
 * agreement. The new cases below pin that down; the cases above keep
 * exercising the global _iq backend and are left untouched.
 *
 * Note: with IQmath's default GLOBAL_IQ the global _iq type is Q24, which is
 * distinct from the _iq8 and _iq15 types used here, so all three can live in
 * this translation unit at the same time.
 */

/* Q8 keeps only 8 fractional bits, so its quantization step is 1/256. */
#define TEST_IQ8_EPS (1e-2f)

TEST_CASE("IQmath: Q8 and Q15 backends coexist", "[clarke_park][iq][multiformat]")
{
    /* The same physical coordinate, expressed in two different Q-formats.
     * Each backend must produce the value in its own format. */
    clarke_park_uvw_iq8_t uvw8 = { .u = _IQ8(1.0f), .v = _IQ8(-0.5f), .w = _IQ8(-0.5f) };
    clarke_park_uvw_iq15_t uvw15 = { .u = _IQ15(1.0f), .v = _IQ15(-0.5f), .w = _IQ15(-0.5f) };

    clarke_park_ab_iq8_t ab8 = {};
    clarke_park_ab_iq15_t ab15 = {};

    clarke_park_clarke(&uvw8, &ab8);
    clarke_park_clarke(&uvw15, &ab15);

    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ8_EPS, 1.0f, _IQ8toF(ab8.alpha));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ8_EPS, 0.0f, _IQ8toF(ab8.beta));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 1.0f, _IQ15toF(ab15.alpha));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.0f, _IQ15toF(ab15.beta));

    /* Park with the same angle, again in both formats. */
    clarke_park_dq_iq8_t dq8 = {};
    clarke_park_dq_iq15_t dq15 = {};

    clarke_park_park(_IQ8(TEST_THETA), &ab8, &dq8);
    clarke_park_park(_IQ15(TEST_THETA), &ab15, &dq15);

    const double d_exp = 1.0 * cos(TEST_THETA) + 0.0 * sin(TEST_THETA);
    const double q_exp = -1.0 * sin(TEST_THETA) + 0.0 * cos(TEST_THETA);
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ8_EPS, d_exp, _IQ8toF(dq8.d));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ8_EPS, q_exp, _IQ8toF(dq8.q));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, d_exp, _IQ15toF(dq15.d));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, q_exp, _IQ15toF(dq15.q));

    /* The reverse transforms stay pairwise independent as well. */
    clarke_park_uvw_iq8_t uvw8_out = {};
    clarke_park_uvw_iq15_t uvw15_out = {};

    clarke_park_iclarke(&ab8, &uvw8_out);
    clarke_park_iclarke(&ab15, &uvw15_out);
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ8_EPS, _IQ8toF(uvw8.u), _IQ8toF(uvw8_out.u));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, _IQ15toF(uvw15.u), _IQ15toF(uvw15_out.u));
}

TEST_CASE("IQmath: precision scales with the Q-format", "[clarke_park][iq][multiformat]")
{
    /* A small value is representable in Q15 but not in Q8, which pins down
     * that the two coordinate types really use different arithmetic. */
    clarke_park_uvw_iq8_t uvw8 = { .u = _IQ8(0.001f), .v = _IQ8(0.0f), .w = _IQ8(0.0f) };
    clarke_park_uvw_iq15_t uvw15 = { .u = _IQ15(0.001f), .v = _IQ15(0.0f), .w = _IQ15(0.0f) };

    clarke_park_ab_iq8_t ab8 = {};
    clarke_park_ab_iq15_t ab15 = {};

    clarke_park_clarke(&uvw8, &ab8);
    clarke_park_clarke(&uvw15, &ab15);

    /* 0.001 is below the Q8 resolution of 1/256, so it collapses to 0. */
    TEST_ASSERT_EQUAL_FLOAT(0.0f, _IQ8toF(ab8.alpha));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.001f * (2.0f / 3.0f), _IQ15toF(ab15.alpha));
}

TEST_CASE("IQmath: every Q-format backend", "[clarke_park][iq][allformats]")
{
    /* Scale the raw IQ word rather than calling _IQNtoF: IQmath's converter
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
        clarke_park_ab_iq##_q##_t ab = {};                                               \
        clarke_park_clarke(&uvw, &ab);                                                   \
        TEST_ASSERT_FLOAT_WITHIN((_q) < 8 ? 0.6f : 2e-2f, 1.0f,                          \
                                 CLARKE_PARK_TEST_IQ_TO_F(_q, ab.alpha));                \
        clarke_park_dq_iq##_q##_t dq = {};                                               \
        clarke_park_park(_IQ##_q(0.0f), &ab, &dq);                                       \
        if ((_q) >= 8) {                                                                 \
            TEST_ASSERT_FLOAT_WITHIN(2e-2f, CLARKE_PARK_TEST_IQ_TO_F(_q, ab.alpha),       \
                                     CLARKE_PARK_TEST_IQ_TO_F(_q, dq.d));                \
            TEST_ASSERT_FLOAT_WITHIN(2e-2f, CLARKE_PARK_TEST_IQ_TO_F(_q, ab.beta),        \
                                     CLARKE_PARK_TEST_IQ_TO_F(_q, dq.q));                \
        }                                                                                \
        clarke_park_ab_iq##_q##_t ab_out = {};                                           \
        clarke_park_ipark(_IQ##_q(0.0f), &dq, &ab_out);                                  \
        if ((_q) >= 8) {                                                                 \
            TEST_ASSERT_FLOAT_WITHIN(2e-2f, CLARKE_PARK_TEST_IQ_TO_F(_q, ab.alpha),       \
                                     CLARKE_PARK_TEST_IQ_TO_F(_q, ab_out.alpha));        \
        }                                                                                \
        clarke_park_uvw_iq##_q##_t uvw_out = {};                                         \
        clarke_park_iclarke(&ab, &uvw_out);                                              \
        TEST_ASSERT_FLOAT_WITHIN((_q) < 8 ? 0.6f : 2e-2f,                                \
                                 CLARKE_PARK_TEST_IQ_TO_F(_q, uvw.u),                     \
                                 CLARKE_PARK_TEST_IQ_TO_F(_q, uvw_out.u));                \
    } while (0);
    CLARKE_PARK_IQ_FORMATS(CLARKE_PARK_IQ_SMOKE)
#undef CLARKE_PARK_IQ_SMOKE
#undef CLARKE_PARK_TEST_IQ_TO_F
}
