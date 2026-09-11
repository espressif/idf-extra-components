/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include "clarke_park.h"

void clarke_park_clarke_iq(const clarke_park_uvw_iq_t *uvw, clarke_park_ab_iq_t *ab)
{
    /* alpha = (2/3) * (u - (v + w)/2),  beta = (v - w)/sqrt(3) */
    ab->alpha = _IQmpy(uvw->u - _IQdiv2(uvw->v + uvw->w), _IQ(2.0f / 3.0f));
    ab->beta = _IQmpy(uvw->v - uvw->w, _IQ(1.0f / M_SQRT3));
}

void clarke_park_iclarke_iq(const clarke_park_ab_iq_t *ab, clarke_park_uvw_iq_t *uvw)
{
    /* u = alpha,  v = (sqrt(3) * beta - alpha)/2,  w = -u - v */
    uvw->u = ab->alpha;
    uvw->v = _IQdiv2(_IQmpy(ab->beta, _IQ(M_SQRT3)) - ab->alpha);
    uvw->w = -uvw->u - uvw->v;
}

/*
 * The Park transforms need sin/cos. When CONFIG_CLARKE_PARK_USE_CORDIC_HW is
 * enabled, these functions are provided by clarke_park_cordic.c instead
 * (which uses the CORDIC hardware for GLOBAL_IQ == 15 and falls back to
 * IQmath otherwise), so they must not be compiled twice.
 */
#if !defined(CONFIG_CLARKE_PARK_USE_CORDIC_HW) || !CONFIG_CLARKE_PARK_USE_CORDIC_HW

void clarke_park_park_iq(_iq theta_rad, const clarke_park_ab_iq_t *ab, clarke_park_dq_iq_t *dq)
{
    _iq sin_theta = _IQsin(theta_rad);
    _iq cos_theta = _IQcos(theta_rad);

    dq->d = _IQmpy(ab->alpha, cos_theta) + _IQmpy(ab->beta, sin_theta);
    dq->q = -_IQmpy(ab->alpha, sin_theta) + _IQmpy(ab->beta, cos_theta);
}

void clarke_park_ipark_iq(_iq theta_rad, const clarke_park_dq_iq_t *dq, clarke_park_ab_iq_t *ab)
{
    _iq sin_theta = _IQsin(theta_rad);
    _iq cos_theta = _IQcos(theta_rad);

    ab->alpha = _IQmpy(dq->d, cos_theta) - _IQmpy(dq->q, sin_theta);
    ab->beta = _IQmpy(dq->d, sin_theta) + _IQmpy(dq->q, cos_theta);
}

#endif // !CONFIG_CLARKE_PARK_USE_CORDIC_HW
