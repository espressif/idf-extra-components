/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <math.h>
#include "soc/soc_caps.h"
#include "esp_err.h"
#include "clarke_park.h"

/* glibc's math.h (linux target) does not define M_SQRT3, newlib does. */
#ifndef M_SQRT3
#define M_SQRT3 1.73205080756887719000
#endif

#define CLARKE_PARK_K1 (2.0f / 3.0f)    /*!< 2/3, the equal-amplitude (amplitude-invariant) coefficient */
#define CLARKE_PARK_K3 (1.0f / M_SQRT3) /*!< 1/sqrt(3) */

#if SOC_CORDIC_SUPPORTED
#define CLARKE_PARK_WITH_CORDIC 1
#else
#define CLARKE_PARK_WITH_CORDIC 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Q15 sin/cos dispatch and backend selection. */
typedef void (*clarke_park_sincos_func_t)(_iq15 theta_rad, _iq15 *sin_theta, _iq15 *cos_theta);
void clarke_park_sincos_q15(_iq15 theta_rad, _iq15 *sin_theta, _iq15 *cos_theta);
void clarke_park_sincos_set_func(clarke_park_sincos_func_t func);

/* IQmath helpers. Q is pasted onto the IQmath API names to select a format. */
#define CLARKE_PARK_IQ_DIV2(_v) _IQdiv2(_v)

#define CLARKE_PARK_IQ_MPY(_q, _a, _b) CLARKE_PARK_IQ_MPY_I(_q, _a, _b)
#define CLARKE_PARK_IQ_MPY_I(_q, _a, _b) _IQ##_q##mpy(_a, _b)

#define CLARKE_PARK_IQ_SIN(_q, _v) CLARKE_PARK_IQ_SIN_I(_q, _v)
#define CLARKE_PARK_IQ_SIN_I(_q, _v) _IQ##_q##sin(_v)

#define CLARKE_PARK_IQ_COS(_q, _v) CLARKE_PARK_IQ_COS_I(_q, _v)
#define CLARKE_PARK_IQ_COS_I(_q, _v) _IQ##_q##cos(_v)

#define CLARKE_PARK_IQ_FROM_FLOAT(_q, _v) CLARKE_PARK_IQ_FROM_FLOAT_I(_q, _v)
#define CLARKE_PARK_IQ_FROM_FLOAT_I(_q, _v) _IQ##_q(_v)

void clarke_park_cordic_sincos_q15(_iq15 theta_rad, _iq15 *sin_theta, _iq15 *cos_theta);

#ifdef __cplusplus
} // extern "C"
#endif
