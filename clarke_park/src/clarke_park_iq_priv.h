/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "clarke_park.h"
#include "clarke_park_private.h"

/*
 * Backend helpers. Q is a decimal token pasted onto _IQNmpy / _IQNsin /
 * _IQNcos. Unused formats are dropped by --gc-sections.
 */

typedef int32_t clarke_park_iq_raw_t;

#define CLARKE_PARK_IQ_AS(_v) ((clarke_park_iq_raw_t)(_v))

#define CLARKE_PARK_IQ_DIV2(_v) (CLARKE_PARK_IQ_AS(_v) >> 1)

#define CLARKE_PARK_IQ_MPY(_q, _a, _b) CLARKE_PARK_IQ_MPY_I(_q, _a, _b)
#define CLARKE_PARK_IQ_MPY_I(_q, _a, _b) \
    CLARKE_PARK_IQ_AS(_IQ##_q##mpy(CLARKE_PARK_IQ_AS(_a), CLARKE_PARK_IQ_AS(_b)))

#define CLARKE_PARK_IQ_SIN(_q, _v) CLARKE_PARK_IQ_SIN_I(_q, _v)
#define CLARKE_PARK_IQ_SIN_I(_q, _v) CLARKE_PARK_IQ_AS(_IQ##_q##sin(CLARKE_PARK_IQ_AS(_v)))

#define CLARKE_PARK_IQ_COS(_q, _v) CLARKE_PARK_IQ_COS_I(_q, _v)
#define CLARKE_PARK_IQ_COS_I(_q, _v) CLARKE_PARK_IQ_AS(_IQ##_q##cos(CLARKE_PARK_IQ_AS(_v)))

#define CLARKE_PARK_IQ_FROM_FLOAT(_q, _v) \
    ((clarke_park_iq_raw_t)((_v) * (float)((int32_t)1 << (_q))))
