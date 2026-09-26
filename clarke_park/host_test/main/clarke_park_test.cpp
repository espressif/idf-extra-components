/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * clarke_park C++ host tests: IQmath algorithm via C++ overloads.
 * Float is a single dispatch smoke; float algorithm coverage lives in
 * clarke_park_test.c.
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

/*
 * Raw _iqN word to float. Do not use _IQNtoF here: on the host it wraps
 * values just below 1.0 into 2.0 for Q25..Q29.
 */
#define CLARKE_PARK_TEST_IQ_TO_F(_q, _v) ((float)(_v) / (float)((int32_t)1 << (_q)))

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

    /* beta = (v - w) / sqrt(3); (0, 1, -1) must not collapse to the alpha axis */
    uvw = { .u = _IQ(0.0f), .v = _IQ(1.0f), .w = _IQ(-1.0f) };
    clarke_park_clarke(&uvw, &ab);
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.0f, _IQtoF(ab.alpha));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 2.0 / sqrt(3.0), _IQtoF(ab.beta));
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

    /* Reference values are computed in double precision so float rounding
     * of the expression does not eat into the IQ quantization budget. */
    const double d_exp = 0.6 * cos(TEST_THETA) + 0.3 * sin(TEST_THETA);
    const double q_exp = -0.6 * sin(TEST_THETA) + 0.3 * cos(TEST_THETA);

    clarke_park_park(_IQ(TEST_THETA), &ab, &dq);
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, d_exp, _IQtoF(dq.d));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, q_exp, _IQtoF(dq.q));

    /* theta = 0 must be the identity transform */
    clarke_park_park(_IQ(0.0f), &ab, &dq);
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, _IQtoF(ab.alpha), _IQtoF(dq.d));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, _IQtoF(ab.beta), _IQtoF(dq.q));

    /* theta = pi/2: (1, 0) -> (0, -1), locks the rotation sense */
    ab = { .alpha = _IQ(1.0f), .beta = _IQ(0.0f) };
    clarke_park_park(_IQ(TEST_HALF_PI), &ab, &dq);
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.0f, _IQtoF(dq.d));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, -1.0f, _IQtoF(dq.q));
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

TEST_CASE("C++ overload API: float dispatch", "[clarke_park][generic][float]")
{
    /* Float algorithm coverage lives in clarke_park_test.c. This case
     * only pins down that the C++ overloads pick the float backend. */
    clarke_park_uvw_f_t uvw = { .u = 1.0f, .v = -0.5f, .w = -0.5f };
    clarke_park_ab_f_t ab = {};

    clarke_park_clarke(&uvw, &ab);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, 1.0f, ab.alpha);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, 0.0f, ab.beta);
}

TEST_CASE("IQmath: every Q-format backend", "[clarke_park][iq][allformats]")
{
    /* Park+iPark at theta=0 applies _IQNcos(0) twice. The IQ31 table
     * cannot hold 1.0, so the compound error is ~2/2^q and exceeds
     * TEST_IQ_EPS below Q12. */
#define TEST_IQ_Q_EPS(_q) ((_q) < 12 ? TEST_IQ8_EPS : TEST_IQ_EPS)
#define CLARKE_PARK_IQ_SMOKE(_q)                                              \
    do {                                                                      \
        clarke_park_uvw_iq##_q##_t uvw = {                                    \
            .u = _IQ##_q(1.0f), .v = _IQ##_q(-0.5f), .w = _IQ##_q(-0.5f)      \
        };                                                                    \
        clarke_park_ab_iq##_q##_t ab = {};                                    \
        clarke_park_clarke(&uvw, &ab);                                        \
        TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_Q_EPS(_q), 1.0f,                     \
                                 CLARKE_PARK_TEST_IQ_TO_F(_q, ab.alpha));     \
        clarke_park_dq_iq##_q##_t dq = {};                                    \
        clarke_park_park(_IQ##_q(0.0f), &ab, &dq);                            \
        TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_Q_EPS(_q),                           \
                                 CLARKE_PARK_TEST_IQ_TO_F(_q, ab.alpha),      \
                                 CLARKE_PARK_TEST_IQ_TO_F(_q, dq.d));         \
        clarke_park_ab_iq##_q##_t ab_out = {};                                \
        clarke_park_ipark(_IQ##_q(0.0f), &dq, &ab_out);                       \
        TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_Q_EPS(_q),                           \
                                 CLARKE_PARK_TEST_IQ_TO_F(_q, ab.alpha),      \
                                 CLARKE_PARK_TEST_IQ_TO_F(_q, ab_out.alpha)); \
        clarke_park_uvw_iq##_q##_t uvw_out = {};                              \
        clarke_park_iclarke(&ab, &uvw_out);                                   \
        TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_Q_EPS(_q),                           \
                                 CLARKE_PARK_TEST_IQ_TO_F(_q, uvw.u),         \
                                 CLARKE_PARK_TEST_IQ_TO_F(_q, uvw_out.u));    \
    } while (0);
    CLARKE_PARK_IQ_FORMATS(CLARKE_PARK_IQ_SMOKE)
#undef CLARKE_PARK_IQ_SMOKE
#undef TEST_IQ_Q_EPS
}
