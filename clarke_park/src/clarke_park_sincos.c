/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "clarke_park.h"
#include "clarke_park_private.h"

/*
 * Keep the default Q15 implementation in an object that has no CORDIC
 * references. This is important: an application that only uses IQmath must not
 * pull the CORDIC backend in merely by using the Q15 transform.
 */
static void clarke_park_sincos_iqmath_q15(_iq15 theta_rad, _iq15 *sin_theta, _iq15 *cos_theta)
{
    *sin_theta = CLARKE_PARK_IQ_SIN(15, theta_rad);
    *cos_theta = CLARKE_PARK_IQ_COS(15, theta_rad);
}

static volatile clarke_park_sincos_func_t s_sincos_func = clarke_park_sincos_iqmath_q15;

void clarke_park_sincos_set_func(clarke_park_sincos_func_t func)
{
    s_sincos_func = func != NULL ? func : clarke_park_sincos_iqmath_q15;
}

void clarke_park_sincos_q15(_iq15 theta_rad, _iq15 *sin_theta, _iq15 *cos_theta)
{
    s_sincos_func(theta_rad, sin_theta, cos_theta);
}
