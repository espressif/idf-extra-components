/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include "clarke_park.h"
#include "clarke_park_private.h"

void clarke_park_clarke_f(const clarke_park_uvw_f_t *uvw, clarke_park_ab_f_t *ab)
{
    /* alpha = (2/3) * (u - (v + w) / 2)
     * beta  = (v - w) / sqrt(3)
     */
    ab->alpha = CLARKE_PARK_K1 * (uvw->u - (uvw->v + uvw->w) * 0.5f);
    ab->beta = CLARKE_PARK_K3 * (uvw->v - uvw->w);
}

void clarke_park_iclarke_f(const clarke_park_ab_f_t *ab, clarke_park_uvw_f_t *uvw)
{
    /* u = alpha
     * v = (sqrt(3) * beta - alpha) / 2
     * w = -u - v
     */
    uvw->u = ab->alpha;
    uvw->v = 0.5f * (M_SQRT3 * ab->beta - ab->alpha);
    uvw->w = -uvw->u - uvw->v;
}

void clarke_park_park_f(float theta_rad, const clarke_park_ab_f_t *ab, clarke_park_dq_f_t *dq)
{
    /* d = alpha * cos(theta) + beta * sin(theta)
     * q = -alpha * sin(theta) + beta * cos(theta)
     */
    float sin_theta = sinf(theta_rad);
    float cos_theta = cosf(theta_rad);

    dq->d = ab->alpha * cos_theta + ab->beta * sin_theta;
    dq->q = -ab->alpha * sin_theta + ab->beta * cos_theta;
}

void clarke_park_ipark_f(float theta_rad, const clarke_park_dq_f_t *dq, clarke_park_ab_f_t *ab)
{
    /* alpha = d * cos(theta) - q * sin(theta)
     * beta  = d * sin(theta) + q * cos(theta)
     */
    float sin_theta = sinf(theta_rad);
    float cos_theta = cosf(theta_rad);

    ab->alpha = dq->d * cos_theta - dq->q * sin_theta;
    ab->beta = dq->d * sin_theta + dq->q * cos_theta;
}
