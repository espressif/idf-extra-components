/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "clarke_park_iq_priv.h"

/*
 * One algorithm body, instantiated for every Q in CLARKE_PARK_IQ_FORMATS.
 * _IQNmpy/_IQNsin/_IQNcos resolve at compile time. The unsuffixed _iq API is
 * a header wrapper around this TU's GLOBAL_IQ; compiling it here would freeze
 * GLOBAL_IQ at 24. Q30 is not instantiated: IQmath has no radian sin/cos.
 */

#define CLARKE_PARK_IQ_DEFINE(_q)                                                               \
                                                                                                \
    void clarke_park_clarke_iq##_q(const clarke_park_uvw_iq##_q##_t *uvw,                       \
                                   clarke_park_ab_iq##_q##_t *ab)                               \
    {                                                                                           \
        /* alpha = (2/3) * (u - (v + w) / 2)                                                    \
         * beta  = (v - w) / sqrt(3)                                                            \
         */                                                                                     \
        ab->alpha = CLARKE_PARK_IQ_MPY(_q,                                                      \
                                       CLARKE_PARK_IQ_AS(uvw->u) -                              \
                                       CLARKE_PARK_IQ_DIV2(uvw->v + uvw->w),                    \
                                       CLARKE_PARK_IQ_FROM_FLOAT(_q, CLARKE_PARK_K1));          \
        ab->beta = CLARKE_PARK_IQ_MPY(_q,                                                       \
                                      CLARKE_PARK_IQ_AS(uvw->v) - CLARKE_PARK_IQ_AS(uvw->w),     \
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
        uvw->u = CLARKE_PARK_IQ_AS(ab->alpha);                                                  \
        uvw->v = CLARKE_PARK_IQ_DIV2(                                                           \
            CLARKE_PARK_IQ_MPY(_q, CLARKE_PARK_IQ_AS(ab->beta),                                 \
                               CLARKE_PARK_IQ_FROM_FLOAT(_q, M_SQRT3)) -                        \
            CLARKE_PARK_IQ_AS(ab->alpha));                                                      \
        uvw->w = -CLARKE_PARK_IQ_AS(uvw->u) - CLARKE_PARK_IQ_AS(uvw->v);                        \
    }                                                                                           \
                                                                                                \
    void clarke_park_park_iq##_q(_iq##_q theta_rad,                                             \
                                 const clarke_park_ab_iq##_q##_t *ab,                           \
                                 clarke_park_dq_iq##_q##_t *dq)                                 \
    {                                                                                           \
        /* d = alpha * cos(theta) + beta * sin(theta)                                           \
         * q = -alpha * sin(theta) + beta * cos(theta)                                          \
         */                                                                                     \
        clarke_park_iq_raw_t sin_theta = CLARKE_PARK_IQ_SIN(_q, theta_rad);                     \
        clarke_park_iq_raw_t cos_theta = CLARKE_PARK_IQ_COS(_q, theta_rad);                     \
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
        clarke_park_iq_raw_t sin_theta = CLARKE_PARK_IQ_SIN(_q, theta_rad);                     \
        clarke_park_iq_raw_t cos_theta = CLARKE_PARK_IQ_COS(_q, theta_rad);                     \
                                                                                                \
        ab->alpha = CLARKE_PARK_IQ_MPY(_q, dq->d, cos_theta) -                                  \
                    CLARKE_PARK_IQ_MPY(_q, dq->q, sin_theta);                                   \
        ab->beta = CLARKE_PARK_IQ_MPY(_q, dq->d, sin_theta) +                                   \
                   CLARKE_PARK_IQ_MPY(_q, dq->q, cos_theta);                                    \
    }

CLARKE_PARK_IQ_FORMATS(CLARKE_PARK_IQ_DEFINE)
#undef CLARKE_PARK_IQ_DEFINE
