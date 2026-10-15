/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "clarke_park_private.h"

/*
 * One algorithm body, instantiated for every Q in CLARKE_PARK_IQ_FORMATS.
 * _IQN()/_IQNmpy/_IQNsin/_IQNcos resolve at compile time. The unsuffixed _iq API is
 * a header wrapper around this TU's GLOBAL_IQ.
 *
 * Q15 is the one format whose sin/cos can also come from the CORDIC hardware.
 * The format is instantiated like every other one; only its sin/cos arm differs,
 * and the choice between the two implementations is made at run time through
 * clarke_park_enable_cordic() or clarke_park_disable_cordic().
 */

/* The X-macro list selects the backend once per format: only Q15 can switch
 * to CORDIC at runtime; all other formats use IQmath directly. */
#define CLARKE_PARK_IQ_SINCOS_SW(_q, _theta, _sin, _cos)   \
    do {                                                   \
        *(_sin) = CLARKE_PARK_IQ_SIN(_q, _theta);          \
        *(_cos) = CLARKE_PARK_IQ_COS(_q, _theta);          \
    } while (0)

#define CLARKE_PARK_IQ_SINCOS_Q15(_q, _theta, _sin, _cos)  \
    clarke_park_sincos_q15((_theta), (_sin), (_cos))

#define CLARKE_PARK_IQ_DEFINE_NON_Q15(_q) \
    CLARKE_PARK_IQ_DEFINE_WITH_SINCOS(_q, CLARKE_PARK_IQ_SINCOS_SW)
#define CLARKE_PARK_IQ_DEFINE_Q15(_q) \
    CLARKE_PARK_IQ_DEFINE_WITH_SINCOS(_q, CLARKE_PARK_IQ_SINCOS_Q15)

#define CLARKE_PARK_IQ_DEFINE_WITH_SINCOS(_q, _sincos)                                          \
                                                                                                \
    void clarke_park_clarke_iq##_q(const clarke_park_uvw_iq##_q##_t *uvw,                       \
                                   clarke_park_ab_iq##_q##_t *ab)                               \
    {                                                                                           \
        /* alpha = (2/3) * (u - (v + w) / 2)                                                    \
         * beta  = (v - w) / sqrt(3)                                                            \
         */                                                                                     \
        ab->alpha = CLARKE_PARK_IQ_MPY(_q,                                                      \
                                       uvw->u - CLARKE_PARK_IQ_DIV2(uvw->v + uvw->w),           \
                                       CLARKE_PARK_IQ_FROM_FLOAT(_q, CLARKE_PARK_K1));          \
        ab->beta = CLARKE_PARK_IQ_MPY(_q,                                                       \
                                      uvw->v - uvw->w,                                          \
                                      CLARKE_PARK_IQ_FROM_FLOAT(_q, CLARKE_PARK_K3));           \
    }                                                                                           \
                                                                                                \
    void clarke_park_iclarke_iq##_q(const clarke_park_ab_iq##_q##_t *ab,                        \
                                    clarke_park_uvw_iq##_q##_t *uvw)                            \
    {                                                                                           \
        /* u = alpha                                                                            \
         * v = (sqrt(3) * beta - alpha) / 2                                                     \
         * w = -u - v                                                                           \
         */                                                                                     \
        uvw->u = ab->alpha;                                                                     \
        uvw->v = CLARKE_PARK_IQ_DIV2(                                                           \
            CLARKE_PARK_IQ_MPY(_q, ab->beta, CLARKE_PARK_IQ_FROM_FLOAT(_q, M_SQRT3)) -          \
            ab->alpha);                                                                         \
        uvw->w = -uvw->u - uvw->v;                                                              \
    }                                                                                           \
                                                                                                \
    void clarke_park_park_iq##_q(_iq##_q theta_rad,                                             \
                                 const clarke_park_ab_iq##_q##_t *ab,                           \
                                 clarke_park_dq_iq##_q##_t *dq)                                 \
    {                                                                                           \
        /* d = alpha * cos(theta) + beta * sin(theta)                                           \
         * q = -alpha * sin(theta) + beta * cos(theta)                                          \
         */                                                                                     \
        _iq##_q sin_theta;                                                                      \
        _iq##_q cos_theta;                                                                      \
        _sincos(_q, theta_rad, &sin_theta, &cos_theta);                                         \
                                                                                                \
        dq->d = CLARKE_PARK_IQ_MPY(_q, ab->alpha, cos_theta) +                                  \
                CLARKE_PARK_IQ_MPY(_q, ab->beta, sin_theta);                                    \
        dq->q = -CLARKE_PARK_IQ_MPY(_q, ab->alpha, sin_theta) +                                 \
                CLARKE_PARK_IQ_MPY(_q, ab->beta, cos_theta);                                    \
    }                                                                                           \
                                                                                                \
    void clarke_park_ipark_iq##_q(_iq##_q theta_rad,                                            \
                                  const clarke_park_dq_iq##_q##_t *dq,                          \
                                  clarke_park_ab_iq##_q##_t *ab)                                \
    {                                                                                           \
        /* alpha = d * cos(theta) - q * sin(theta)                                              \
         * beta  = d * sin(theta) + q * cos(theta)                                              \
         */                                                                                     \
        _iq##_q sin_theta;                                                                      \
        _iq##_q cos_theta;                                                                      \
        _sincos(_q, theta_rad, &sin_theta, &cos_theta);                                         \
                                                                                                \
        ab->alpha = CLARKE_PARK_IQ_MPY(_q, dq->d, cos_theta) -                                  \
                    CLARKE_PARK_IQ_MPY(_q, dq->q, sin_theta);                                   \
        ab->beta = CLARKE_PARK_IQ_MPY(_q, dq->d, sin_theta) +                                   \
                   CLARKE_PARK_IQ_MPY(_q, dq->q, cos_theta);                                    \
    }

CLARKE_PARK_IQ_FORMATS_WITH_Q15(CLARKE_PARK_IQ_DEFINE_NON_Q15, CLARKE_PARK_IQ_DEFINE_Q15)
